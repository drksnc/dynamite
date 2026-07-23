#ifndef HOOK_DYNAMITE_H
#define HOOK_DYNAMITE_H

#include "Config.h"
#include "DynamiteCore.h"
#include "DynamiteSyncImpl/DynamiteSyncImpl.h"
#include "Tpp/TppNPCLifeState.h"
#include "Tpp/TppTypes.h"
#include "Version.h"
#include "lua/lua.h"
#include "patch.h"
#include "spdlog/logger.h"
#include "windows.h"
#include "D3D11Hook.hpp"
#include "WindowsMessageHook.hpp"

#include <eh.h>
#include <filesystem>
#include <map>
#include <memory>

namespace Dynamite {
    static const std::filesystem::path logPath = std::filesystem::path("dynamite") / std::filesystem::path("dynamite.log.txt");

    void TerminateHandler();
    inline terminate_function terminate_Original;
    static const std::string logName = "dynamite";
    inline std::map<std::string, uint64_t> addressSet;

    class Dynamite {
      public:
        Dynamite();
        ~Dynamite();

        void SetupLogger();

        void RebaseAddresses();

        void CreateHooks();

        void CreateDebugHooks() const;

        void CreateD3DHook();

        void ReadConfig();

        void SetFuncPtrs();

        void OnFrame();

        void OnReset();

        bool OnMessage(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param);

        void OnUIEvent(Event event);

        void ToggleDrawUI();

        static void AbortHandler(int signal_number);

        static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS * /*ExceptionInfo*/);

        static std::vector<Patch> GetPatches();

        static bool PatchMasterServerURL(const std::string &url);
        static bool PatchSteamNetworking();

        DynamiteSyncImpl dynamiteSyncImpl;

        DynamiteCore dynamiteCore;

        Config cfg{};

        std::map<uint32_t, std::string> messageDict;
        std::map<uint64_t, std::string> pathDict;
      private:

        void DrawUI();

        void DrawMenu();
        void DrawClientMenu();
        void DrawHostMenu();

        bool FrameInitialize();
        void CreateRenderTarget();
        void CleanupRenderTarget();

        static constexpr uint64_t BaseAddr = 0x140000000;
        uint64_t RealBaseAddr = 0;
        HMODULE thisModule{nullptr};
        HWND hwnd{};
        std::shared_ptr<spdlog::logger> log = nullptr;
        std::unique_ptr<D3D11Hook> d3d11Hook{};
        std::unique_ptr<WindowsMessageHook> windowsMessageHook;
        ID3D11RenderTargetView *mainRenderTargetView{nullptr};
         
        // d3d11
        bool firstFrame = true;
        bool frameInitialized = false;
        bool d3dHooked = false;
        bool drawUI = true;
        bool unlockCursor = false;
        bool menuOpen = false; // tex start open, as it's used as a intro and error window during startup
        bool menuOpenPrev;
    };
}

#endif // HOOK_DYNAMITE_H
