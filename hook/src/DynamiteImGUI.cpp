#include "dynamite.h"
#include "spdlog/spdlog.h"

#include <imgui.h>
#include "imguiimpl/imgui_impl_win32.h"
#include "imguiimpl/imgui_impl_dx11.h"

#include "RawInput.h"
#include "DynamiteLua.h"
#include "DynamiteHook.h"
#include "DynamiteSteam.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam); // tex see note in imgui_impl_win32.h

namespace Dynamite {
    void Dynamite::DrawUI()
    {
        auto &io = ImGui::GetIO();
        if (!drawUI) {
            RawInput::UnBlockMouseClick();
            RawInput::UnBlockKeyboard();
            unlockCursor = false;
            io.MouseDrawCursor = false;
            return;
        }

        // tex disable mouse input to game
        if (unlockCursor) {
            ImGui::CaptureMouseFromApp(true);
        }

        if (io.WantCaptureMouse) {
            RawInput::BlockMouseClick();
        } else {
            RawInput::UnBlockMouseClick();
        }

        if (io.WantCaptureKeyboard) {
            RawInput::BlockKeyboard();
        } else {
            RawInput::UnBlockKeyboard();
        }

        io.MouseDrawCursor = unlockCursor;

        if (menuOpen) {
            DrawMenu();
            ImGui::ShowDemoWindow(&menuOpen);
        }

        menuOpenPrev = menuOpen;
    }

    void Dynamite::DrawMenu() {
        ImGui::SetNextWindowSizeConstraints({420, 368}, {800, 600});

        ImGui::Begin("Co-op Manager");

        ImGui::Separator();

        ImGui::Checkbox("Is Host", &g_hook->cfg.Host);

        ImGui::Separator();

        g_hook->cfg.Host ? DrawHostMenu() : DrawClientMenu();

        ImGui::End();

    }

    void Dynamite::DrawHostMenu()
    {
        if (ImGui::Button("Load Mission Manually"))
            g_hook->dynamiteCore.LoadMission(10040);

        ImGui::Text("Status: ");
        ImGui::SameLine();
        g_hook->dynamiteCore.GetHostSessionCreated() ? ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "Hosting")
                                                     : ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, "Idle");

        ImGui::Text("Players: %d", g_hook->dynamiteCore.GetMemberCount());
    }

    void Dynamite::DrawClientMenu()
    {
        ImGui::Text("Status: ");
        ImGui::SameLine();
        if (g_hook->dynamiteCore.GetSessionCreated()) {
            g_hook->dynamiteCore.GetSessionConnected() ? ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "Connected")
                                                       : ImGui::TextColored({1.0f, 1.0f, 0.0f, 1.0f}, "Connecting...");
        }
        else
        {
            ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "Disconnected");
        }

        const int friendsCount = GetSteamFriendsCount();
        static uint64_t selectedFriendSteamId = 0;
        uint64_t prevSelectedFriendSteamId = selectedFriendSteamId;
        if (friendsCount > 0) {
            if (ImGui::TreeNode("Friend list")) {
                static int selected = -1;
                for (int n = 0; n < friendsCount; n++) {

                    uint64_t friendSteamID = GetSteamFriendByIndex(n);
                    if (friendSteamID < 0)
                        continue;

                    const char *pName = GetSteamFriendPersonaName(friendSteamID);
                    if (ImGui::Selectable(pName, selected == n)) {
                        selected = n;
                        selectedFriendSteamId = friendSteamID;
                    }
                }
                ImGui::TreePop();
            }
        }

        static char buf[64];
        static bool isHostCopied = false;

        if (selectedFriendSteamId != prevSelectedFriendSteamId) {
            g_hook->cfg.HostSteamID = selectedFriendSteamId;
            isHostCopied = false;
        }

        if (!isHostCopied) {
            if (g_hook->cfg.HostSteamID > 0)
                std::to_chars(buf, buf + sizeof(buf), g_hook->cfg.HostSteamID);

            isHostCopied = true;
        }

        if (ImGui::InputText("Host SteamID", buf, 64, ImGuiInputTextFlags_CharsDecimal)) {
            uint64_t steamID = std::stoull(buf);
            if (steamID >= 0x110000100000000 && steamID <= 0x01100001FFFFFFFF) {
                g_hook->cfg.HostSteamID = steamID;
            }
        }

        if (g_hook->dynamiteCore.GetSessionCreated())
        {
            if (ImGui::Button("Cancel Connection"))
                l_ResetClientSessionStateWithNotification(hookState.luaState);
        } 
        else 
        {
            if (ImGui::Button("Connect"))
                l_CreateClientSession(hookState.luaState);
        }
    }

    // tex called on initialize and on device reset
    bool Dynamite::FrameInitialize() {
        if (frameInitialized) {
            return true;
        }

        spdlog::info("Attempting to frame initialize");

        auto device = d3d11Hook->get_device();
        auto swapChain = d3d11Hook->get_swap_chain();

        // Wait.
        if (device == nullptr || swapChain == nullptr) {
            spdlog::info("Device or SwapChain null. DirectX 12 may be in use. A crash may occur.");
            return false;
        }

        ID3D11DeviceContext *context = nullptr;
        device->GetImmediateContext(&context);

        DXGI_SWAP_CHAIN_DESC swapDesc{};
        swapChain->GetDesc(&swapDesc);

        hwnd = swapDesc.OutputWindow;

        // RE2FW: Explicitly call destructor first
        windowsMessageHook.reset();
        windowsMessageHook = std::make_unique<WindowsMessageHook>(hwnd);
        windowsMessageHook->on_message = [this](auto wnd, auto msg, auto wParam, auto lParam) { return OnMessage(wnd, msg, wParam, lParam); };

        spdlog::info("Creating render target");

        CreateRenderTarget();

        spdlog::info("Window Handle: {0:x}", (uintptr_t)hwnd);
        spdlog::info("Initializing ImGui");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        spdlog::info("Initializing ImGui Win32");

        if (!ImGui_ImplWin32_Init(hwnd)) {
            spdlog::error("Failed to initialize ImGui.");
            return false;
        }

        spdlog::info("Initializing ImGui D3D11");

        if (!ImGui_ImplDX11_Init(device, context)) {
            spdlog::error("Failed to initialize ImGui.");
            return false;
        }

        ImGui::StyleColorsDark();

        // SaveGuiStyle("styledefaultdump.lua");//DEBUGNOW

        if (firstFrame) {
            firstFrame = false;

            RawInput::InitializeInput();
        } // if firstFrame

        spdlog::info("frame initialized");
        return true;
    } // FrameInitialize

    // D3D11Hook->present
    // GOTCHA: this is blocking to actual d3d Present, so keep performance in mind
    void Dynamite::OnFrame() {
        // spdlog::trace("OnFrame");
        auto frameTimeStart = std::chrono::high_resolution_clock::now();

        // GOTCHA: frameInitialized is reset in OnReset, so if you want something to run only once a session use firstFrame in FramInisialize instead
        if (!frameInitialized) {
            if (!FrameInitialize()) {
                spdlog::error("Failed to frame initialize IHHook");
                return;
            }

            spdlog::info("IHHook frame initialized");
            frameInitialized = true;
            return; // tex give it an extra frame to settle I guess?
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // DEBUGNOW
        // if (m_error.empty() && m_game_data_initialized) {
        //	m_mods->on_frame();
        // }

        // DEBUGNOW test frame impact
        // bool boop = false;
        // for (int i = 0; i < 10000000; i++) {
        //	boop = !boop;
        // }

        DrawUI();

        ImGui::EndFrame();
        ImGui::Render();

        ID3D11DeviceContext *context = nullptr;
        d3d11Hook->get_device()->GetImmediateContext(&context);

        context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        auto frameTimeEnd = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::microseconds>(frameTimeEnd - frameTimeStart).count();
        // spdlog::trace("frame time microseconds: {}", frameDuration);//DEBUGNOW
    } // OnFrame

    // D3D11Hook
    void Dynamite::OnReset() {
        spdlog::info("OnReset");
        // DEBUGNOW
        auto log = spdlog::get("ihhook");
        if (log != NULL) {
            log->flush();
        }

        // RE2FW: Crashes if we don't release it at this point.
        CleanupRenderTarget();
        frameInitialized = false;

        // DEBUGNOW
        spdlog::info("OnReset done");
        if (log != NULL) {
            log->flush();
        }

    } // OnReset

    	// WindowsMessageHook
    bool Dynamite::OnMessage(HWND wnd, UINT message, WPARAM w_param, LPARAM l_param) {
        // spdlog::trace("OnMessage");

        if (!frameInitialized) {
            return true;
        }

        bool handledMessage = !RawInput::OnMessage(wnd, message, w_param, l_param);

        if (drawUI && ImGui_ImplWin32_WndProcHandler(wnd, message, w_param, l_param) != 0) {
            // RE2FW: If the user is interacting with the UI we block the message from going to the game.
            auto &io = ImGui::GetIO();

            if (io.WantCaptureMouse || io.WantCaptureKeyboard || io.WantTextInput) {
                handledMessage = true;
            }
        }

        if (handledMessage) {
            // tex DEBUGNOW WORKAROUND: having menu eat all game can cause a problem if user was holding a key at the time as the keyup even will be eaten
            if (w_param == WM_KEYUP) {
                return true;
            }

            return false; // tex eat the message
        }
        return true;
    } // OnMessage

    void Dynamite::CreateRenderTarget() {
        CleanupRenderTarget();

        ID3D11Texture2D *backBuffer{nullptr};
        if (d3d11Hook->get_swap_chain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&backBuffer) == S_OK) {
            d3d11Hook->get_device()->CreateRenderTargetView(backBuffer, NULL, &mainRenderTargetView);
            backBuffer->Release();
        }
    } // CreateRenderTarget

    void Dynamite::CleanupRenderTarget() {
        spdlog::trace("CleanupRenderTarget");
        // DEBUGNOW
        auto log = spdlog::get("ihhook");
        if (log != NULL) {
            log->flush();
        }

        if (mainRenderTargetView != nullptr) {
            mainRenderTargetView->Release();
            mainRenderTargetView = nullptr;
        }
    } // CleanupRenderTarget

    void Dynamite::ToggleDrawUI() 
    {
        menuOpen = !menuOpen;
        unlockCursor = menuOpen;
    }
}