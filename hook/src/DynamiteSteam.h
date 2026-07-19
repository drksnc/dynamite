#pragma once
#include <cstdint>

namespace Dynamite {
    class ISteamFriends;

    using SteamFriends = ISteamFriends *(__cdecl *)();
    using SteamGetFriendCount = int (__cdecl *)(ISteamFriends *, int);
    using SteamGetFriendByIndex = uint64_t (__cdecl *)(ISteamFriends *, int iFriend, int iFriendFlags);
    using SteamGetFriendPersonaName = const char * (__cdecl *)(ISteamFriends *, uint64_t friendSteamID);


    void LoadSteam();
    int GetSteamFriendsCount();
    uint64_t GetSteamFriendByIndex(int idx);
    const char * GetSteamFriendPersonaName(uint64_t steamID);
}