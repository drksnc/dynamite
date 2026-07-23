#pragma once
#include <steamnetworkingsockets.h>
#include <mutex>
#include <atomic>

class CSteamClient
{
public:
    CSteamClient();

    void Connect(const char *ip_address);
    void Disconnect();
    void Update();

    bool IsConnected() const { return m_bConnected; }

    void SendNetworkMessageToServer(void *data, const size_t size, const bool reliable);
private:

	static CSteamClient *s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *pInfo) {
      s_pCallbackInstance->OnSteamNetConnectionStatusChanged(pInfo);
    }

    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo);

    void PollMessages();

    std::atomic<bool> m_bConnected = false;
    std::atomic<bool> m_bWaitingForServerRespond = false;
    HSteamNetConnection m_hConnection = k_HSteamNetConnection_Invalid;
    ISteamNetworkingSockets *m_pNET = nullptr;

   	std::mutex m_handlerLock;
    std::thread m_thNetUpdate;
};