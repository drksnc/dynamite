#ifndef HOOK_DYNAMITECORE_H
#define HOOK_DYNAMITECORE_H
#include "BossQuietNextActionTaskActionCondition.h"
#include "Config.h"
#include "EmblemInfo.h"
#include "ScriptVarResult.h"
#include "Tpp/BossQuietActionTask.h"
#include "Tpp/TppNPCLifeState.h"
#include "Tpp/TppTypes.h"
#include "ThreadSafeStack.hpp"
#include "SteamClient.h"
#include "SteamServer.h"
#include "net_queue.h"

#include <deque>
#include <mutex>
#include <thread>

namespace Dynamite {
    class DynamiteCore {
      public:
        DynamiteCore();
        void WithConfig(Config *);

        int GetMemberCount() const;
        int GetNearestPlayer() const;
        static Vector3 GetPlayerPosition(int index);
        static ENPCLifeState GetSoldierLifeStatus(int objectID);
        static Vector3 GetSoldierPosition(int objectID);
        static EmblemInfo GetEmblemInfo();
        static ScriptVarResult GetSVar(const std::string &catName, const std::string &varName);
        static void *GetEmblemEditorSystemImpl();
        void CreateEmblem(EmblemInfo info);
        void RemoveOpponentEmblemTexture();
        void StartNearestEnemyThread();
        void StopNearestEnemyThread();
        bool IsNearestEnemyThreadRunning() const;
        unsigned int GetOffensePlayerID() const;
        unsigned int GetDefensePlayerID() const;
        void ResetState();
        void SetSessionCreated(bool v);
        bool GetSessionCreated() const;
        void SetHostSessionCreated(bool v);
        bool GetHostSessionCreated() const;
        void SetSessionConnected(bool v);
        bool GetSessionConnected() const;
        void SetEmblemCreated(bool v);
        bool GetEmblemCreated() const;
        void MissionComplete();
        uint32_t GetMissionsCompleted() const;
        static unsigned short GetActiveEquipmentID(uint32_t playerID);
        static unsigned short GetEquipIDInSlot(uint32_t playerID, uint32_t slotID, uint32_t index);
        void BossQuietSetNextActionTask(uint32_t param_1, BossQuietActionTask *actionTask, BossQuietNextActionTaskActionCondition actionType) const;
        short GetCurrentMissionID();
        std::string GetCurrentMissionName();
        void LoadMission(short missionId);
        void TestFunction();

        void StartSteamtHost();
        void StopSteamtHost();
        void StartSteamClient(const char *ip_address);
        void StopSteamClient();

        void SendNetworkMessage(Packet packet, bool reliable);
        void SendRawNetworkMessage(void *data, const size_t size);
        void SendLocalMessage(Packet packet);

        bool IsHostWorking() const;
        bool IsClientConnected() const;

        void CreateNetworkMessage(void *ptr, const size_t size) { m_NetworkMessages.Create(ptr, size); };
        void RetrieveNetworkMessages();
        bool RetrieveNativeNetworkMessages(void *steam_ptr, void *dest, uint32_t destSize, uint32_t *bytesRead);
        uint32_t HasNativePacket();
        void OnClientConnectFinished(const bool isLocal);


      private:
        bool sessionCreated = false;
        bool sessionConnected = false;
        bool emblemCreated = false;
        bool hostSessionCreated = false;
        unsigned int offensePlayerID = 0;
        unsigned int defensePlayerID = 15;
        Config *cfg = nullptr;
        std::jthread nearestPlayerThread;
        uint32_t missionsCompleted = 0;
        std::filesystem::path missionsFilename = std::filesystem::path("dynamite") / std::filesystem::path("missions.txt");

        CSteamServer *m_pSteamServer = nullptr;
        CSteamClient *m_pSteamClient = nullptr;
        NetQueue m_NetworkMessages;
        std::mutex m_nativeMutex;
        std::deque<std::vector<uint8_t>> m_nativeQueue;
    };
}

#endif // HOOK_DYNAMITECORE_H
