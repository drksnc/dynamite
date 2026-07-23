#include "SteamServer.h"
#include <isteamnetworkingutils.h>
#include <spdlog/spdlog.h>
#include "DynamiteHook.h"

static constexpr uint16_t PORT = 3421;
static constexpr int TIMEOUT = std::numeric_limits<int>::max();
static constexpr int SEND_BUFFER_SIZE = 1000 * 1024;

CSteamServer::CSteamServer() 
{
	s_pCallbackInstance = this;
}

void CSteamServer::Start() 
{ 
    m_bWorking = false;

    SteamNetworkingIdentity identity;
    identity.Clear();
    identity.SetLocalHost();

    SteamNetworkingErrMsg err;

    if (!GameNetworkingSockets_Init(&identity, err))
        return;

    m_pNET = SteamNetworkingSockets();

	SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_TimeoutInitial, TIMEOUT);
    SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_TimeoutConnected, TIMEOUT);
    SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendBufferSize, SEND_BUFFER_SIZE);
    SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1);

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)SteamNetConnectionStatusChangedCallback);

    SteamNetworkingIPAddr serverLocalAddr;
    serverLocalAddr.Clear();
    serverLocalAddr.m_port = PORT;

    m_hListenSock = m_pNET->CreateListenSocketIP(serverLocalAddr, 1, &opt);

    if (m_hListenSock == k_HSteamListenSocket_Invalid) {
        spdlog::error("failed to listen on port {}", PORT);
        return;
    }

    m_hPollGroup = m_pNET->CreatePollGroup();

    if (m_hPollGroup == k_HSteamNetPollGroup_Invalid) {
        spdlog::error("failed to listen on port {}", PORT);
        return;
    }

    char szAddr[SteamNetworkingIPAddr::k_cchMaxString];
    SteamNetworkingUtils()->SteamNetworkingIPAddr_ToString(serverLocalAddr, szAddr, sizeof(szAddr), false);

    spdlog::info("server running on {} {}", szAddr, PORT);

    m_bWorking = true;

    m_thNetUpdate = std::thread([this]() {
        while (true) {
            {
                const std::lock_guard<std::mutex> lock(m_handlerLock);

                if (!m_bWorking)
                    break;

                Update();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void CSteamServer::Stop() 
{ 
    m_bWorking = false;
    DisconnectClient(m_hClientConnection);

	if (m_hListenSock != k_HSteamListenSocket_Invalid) {
        m_pNET->CloseListenSocket(m_hListenSock);
        m_hListenSock = k_HSteamListenSocket_Invalid;
    }

    if (m_hPollGroup != k_HSteamNetPollGroup_Invalid) {
        m_pNET->DestroyPollGroup(m_hPollGroup);
        m_hPollGroup = k_HSteamNetPollGroup_Invalid;
    }

    if (m_thNetUpdate.joinable())
        m_thNetUpdate.join();

    GameNetworkingSockets_Kill();
}

void CSteamServer::Update() 
{ 
	m_pNET->RunCallbacks();
    PollMessages();
}

void CSteamServer::PollMessages()  
{
    constexpr uint8_t maxMessages = 5;
    ISteamNetworkingMessage *pIncomingMsgs[maxMessages];

    const int numMsgs = m_pNET->ReceiveMessagesOnPollGroup(m_hPollGroup, pIncomingMsgs, maxMessages);

    for (uint8_t i = 0; i < numMsgs; ++i) {
        if (!pIncomingMsgs[i])
            continue;

        const auto size = pIncomingMsgs[i]->m_cbSize;

        if (size >= 1) {
            Dynamite::g_hook->dynamiteCore.CreateNetworkMessage(pIncomingMsgs[i]->m_pData, size);
        }

        pIncomingMsgs[i]->Release();
    }
}

CSteamServer *CSteamServer::s_pCallbackInstance = nullptr;
void CSteamServer::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo) 
{
    //spdlog::info("{} {}", __FUNCTION__, pInfo->m_info.m_eState);

    switch (pInfo->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {
                spdlog::info("client disconnected: {}", pInfo->m_info.m_szConnectionDescription);
                DisconnectClient(pInfo->m_hConn);
            }
        } break;
        case k_ESteamNetworkingConnectionState_Connecting: {
            spdlog::info("connection request: {}", pInfo->m_info.m_szConnectionDescription);
            HandleConnection(pInfo);
        } break;
        case k_ESteamNetworkingConnectionState_Connected: {
            spdlog::info("connection finished: {}", pInfo->m_info.m_szConnectionDescription);
            m_hClientConnection = pInfo->m_hConn;
            Dynamite::g_hook->dynamiteCore.OnClientConnectFinished(false);
            break;
        }
        default:
            break;
    }
}

void CSteamServer::HandleConnection(SteamNetConnectionStatusChangedCallback_t *pInfo) {
    if (m_pNET->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
        spdlog::info("Can't accept connection.");
        DisconnectClient(pInfo->m_hConn);
        return;
    }

    int msgSize = pInfo->m_info.m_identityRemote.m_cbSize;
    if (msgSize > pInfo->m_info.m_identityRemote.k_cbMaxGenericBytes) {
        spdlog::info("Packet too big.");
        DisconnectClient(pInfo->m_hConn);
        return;
    }

    if (msgSize < sizeof(uint32_t)) {
        spdlog::info("Packet too small.");
        DisconnectClient(pInfo->m_hConn);
        return;
    }

    if (!m_pNET->SetConnectionPollGroup(pInfo->m_hConn, m_hPollGroup)) {
        spdlog::info("Failed to set poll group.");
        DisconnectClient(pInfo->m_hConn);
        return;
    }
}

void CSteamServer::SendNetworkMessageToClient(void *data, const size_t size, const bool reliable) {
    const int32_t flags = reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
    m_pNET->SendMessageToConnection(m_hClientConnection, data, size, flags, nullptr);
}

void CSteamServer::DisconnectClient(HSteamNetConnection connection) { 
    m_pNET->CloseConnection(connection, 0, "", false);

    if (connection == m_hClientConnection)
        m_hClientConnection = k_HSteamNetConnection_Invalid;
}