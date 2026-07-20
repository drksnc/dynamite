#include "windows.h"
#include <iostream>

#include "dynamite.h"

#include <spdlog/spdlog.h>

HMODULE g_thisModule;
extern HMODULE origDll; // dinputproxy

#include "DynamiteHook.h"

DWORD WINAPI InitThread(LPVOID lpParameter) {
    Sleep(1000);
    ((Dynamite::Dynamite *)lpParameter)->CreateD3DHook();

    return 0;
} // InitThread


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_thisModule = hModule;
        auto dynamite = new Dynamite::Dynamite();

        HANDLE hInitThread = CreateThread(nullptr, 0, InitThread, dynamite, 0, nullptr);
        if (hInitThread == NULL) {
        
        } else {
            CloseHandle(hInitThread);
        }

    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (origDll) {
            delete Dynamite::g_hook;
            spdlog::shutdown();
            FreeLibrary(origDll);
        }
    }

    return true;
}