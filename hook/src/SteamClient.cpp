#include "SteamClient.h"
#include <isteamnetworkingutils.h>
#include <spdlog/spdlog.h>

static constexpr int TIMEOUT = std::numeric_limits<int>::max();
static constexpr uint16_t PORT = 3421;
static constexpr int SEND_BUFFER_SIZE = 1000 * 1024;

#include "DynamiteHook.h"

CSteamClient::CSteamClient() 
{ 
    s_pCallbackInstance = this;
}

void CSteamClient::Connect(const char *ip_address) 
{ 
    m_bConnected = false;

    SteamNetworkingIdentity identity;
    identity.Clear();

    SteamNetworkingErrMsg err;

    if (!GameNetworkingSockets_Init(&identity, err))
        return;

	SteamNetworkingIPAddr serverAddr;
    serverAddr.Clear();

    SteamNetworkingUtils()->SteamNetworkingIPAddr_ParseString(&serverAddr, ip_address);
    serverAddr.m_port = PORT;

    m_pNET = SteamNetworkingSockets(); 

    spdlog::info("connecting to server {}", ip_address);

    SteamNetworkingConfigValue_t opt{};
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)SteamNetConnectionStatusChangedCallback);

    m_hConnection = m_pNET->ConnectByIPAddress(serverAddr, 1, &opt);

    if (m_hConnection == k_HSteamNetConnection_Invalid)
    {
        spdlog::error("can't connect to server");
        return;
    }

    m_bWaitingForServerRespond = true;

    m_thNetUpdate = std::thread([this]() {
        while (true) {
            {
                const std::lock_guard<std::mutex> lock(m_handlerLock);

                if (!m_bConnected && !m_bWaitingForServerRespond)
                    break;

                Update();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void CSteamClient::Disconnect()
{
    if (!m_bConnected)
        return;

    m_bConnected = false;
    m_bWaitingForServerRespond = false;

    if (m_thNetUpdate.joinable())
        m_thNetUpdate.join();

    m_pNET->FlushMessagesOnConnection(m_hConnection);
    m_pNET->CloseConnection(m_hConnection, ESteamNetConnectionEnd::k_ESteamNetConnectionEnd_App_Generic, "Disconnecting...", false);
    m_pNET = nullptr;
    GameNetworkingSockets_Kill();
    spdlog::info("disconnecting...");
}

void CSteamClient::Update() 
{ 
	m_pNET->RunCallbacks();
    PollMessages();
}

void CSteamClient::SendNetworkMessageToServer(void *data, const size_t size, const bool reliable) 
{ 
    const int32_t flags = reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
    m_pNET->SendMessageToConnection(m_hConnection, data, size, flags, nullptr);
}

void CSteamClient::PollMessages()
{
    ISteamNetworkingMessage *pIncomingMsg = nullptr;
    int numMsgs = m_pNET->ReceiveMessagesOnConnection(m_hConnection, &pIncomingMsg, 1);

    if (numMsgs <= 0)
        return;

    if (pIncomingMsg == nullptr)
        return;

    const auto size = pIncomingMsg->GetSize();

    if (size >= 1) {
        Dynamite::g_hook->dynamiteCore.CreateNetworkMessage(pIncomingMsg->m_pData, size);
    }

    pIncomingMsg->Release();
}

CSteamClient *CSteamClient::s_pCallbackInstance = nullptr;
void CSteamClient::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo) 
{
    //spdlog::info("{} {}", __FUNCTION__ , pInfo->m_info.m_eState);
    //spdlog::info("new client state: {}", pInfo->m_info.m_eState);

    const auto state = pInfo->m_info.m_eState;

    switch (state) {
        case k_ESteamNetworkingConnectionState_Connected: 
        {
            m_bConnected = true;
            m_bWaitingForServerRespond = false;

            Dynamite::g_hook->dynamiteCore.OnClientConnectFinished(true);

        } break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            m_bConnected = false;
            m_bWaitingForServerRespond = false;
        } break;
    }
}