#include "DynamiteSteam.h"
#include "Windows.h"

namespace Dynamite
{
    static SteamFriends friendsInstance = nullptr;
    static SteamGetFriendCount getFriendCountFunc = nullptr;
    static SteamGetFriendByIndex getFriendByIndexFunc = nullptr;
    static SteamGetFriendPersonaName getFriendPersonaNameFunc = nullptr;

    void LoadSteam()
    { 
        HMODULE steamApiHandle = GetModuleHandleA("steam_api64.dll");
        if (steamApiHandle == nullptr) {
            steamApiHandle = LoadLibraryA("steam_api64.dll");
        }

        if (steamApiHandle != nullptr) {
            friendsInstance = (SteamFriends)GetProcAddress(steamApiHandle, "SteamFriends");
            getFriendCountFunc = (SteamGetFriendCount)GetProcAddress(steamApiHandle, "SteamAPI_ISteamFriends_GetFriendCount");
            getFriendByIndexFunc = (SteamGetFriendByIndex)GetProcAddress(steamApiHandle, "SteamAPI_ISteamFriends_GetFriendByIndex");
            getFriendPersonaNameFunc = (SteamGetFriendPersonaName)GetProcAddress(steamApiHandle, "SteamAPI_ISteamFriends_GetFriendPersonaName");
        }
    }

    int GetSteamFriendsCount() {
        if (friendsInstance == nullptr)
            return -1;

        return getFriendCountFunc(friendsInstance(), 0x04);
    }

    uint64_t GetSteamFriendByIndex(int idx) {
        if (friendsInstance == nullptr)
            return -1;

        return getFriendByIndexFunc(friendsInstance(), idx, 0x04);
    }

    const char * GetSteamFriendPersonaName(uint64_t steamID) {
        if (friendsInstance == nullptr)
            return "";

        return getFriendPersonaNameFunc(friendsInstance(), steamID);
    }
}