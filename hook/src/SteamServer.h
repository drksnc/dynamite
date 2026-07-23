#pragma once
#include <steamnetworkingsockets.h>
#include <mutex>
#include <atomic>

class CSteamServer
{
public:
    CSteamServer();

    void Start();
    void Stop();
    void Update();

    bool IsWorking() const { return m_bWorking.load(); };

    void SendNetworkMessageToClient(void *data, const size_t size, const bool reliable);
    void DisconnectClient(HSteamNetConnection connection);

    static CSteamServer *s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *pInfo) {
        s_pCallbackInstance->OnSteamNetConnectionStatusChanged(pInfo);
    }

    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo);
    void HandleConnection(SteamNetConnectionStatusChangedCallback_t *pInfo);

  private:

    void PollMessages();

    std::atomic<bool> m_bWorking = false;
    HSteamListenSocket m_hListenSock = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup m_hPollGroup = k_HSteamNetPollGroup_Invalid;
    HSteamNetConnection m_hClientConnection = k_HSteamNetConnection_Invalid;
    ISteamNetworkingSockets *m_pNET = nullptr;

   	std::mutex m_messageLock;
    std::mutex m_handlerLock;
    std::thread m_thNetUpdate;
};