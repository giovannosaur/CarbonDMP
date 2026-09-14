#include "pch.h"

#include <windows.h>
#include <d3d9.h>
#include <MinHook.h>

#include "d3d_hook.h"

#include "../ui/music_player_ui.h"

#ifdef CARBONDMP_DEBUG
#include "../ui/debug_overlay.h"
#endif

#include "../src/logger.h"

namespace
{
    constexpr uintptr_t CARBON_DEVICE_PTR =
        0x00AB0ABC;

    void* g_endSceneAddress = nullptr;
    void* g_resetAddress = nullptr;

    using EndScene_t =
        HRESULT(__stdcall*)(
            LPDIRECT3DDEVICE9
            );

    using Reset_t =
        HRESULT(__stdcall*)(
            LPDIRECT3DDEVICE9,
            D3DPRESENT_PARAMETERS*
            );

    EndScene_t oEndScene = nullptr;
    Reset_t oReset = nullptr;


    HRESULT __stdcall hkEndScene(
        LPDIRECT3DDEVICE9 device)
    {
#ifdef CARBONDMP_DEBUG
        DebugOverlay::Render(device);
#endif

        MusicPlayerUI::Update();
        MusicPlayerUI::Render(device);

        return oEndScene(device);
    }


    HRESULT __stdcall hkReset(
        LPDIRECT3DDEVICE9 device,
        D3DPRESENT_PARAMETERS* presentationParameters)
    {
#ifdef CARBONDMP_DEBUG
        DebugOverlay::OnLostDevice();
#endif

        MusicPlayerUI::OnLostDevice();

        HRESULT hr =
            oReset(
                device,
                presentationParameters
            );

        if (SUCCEEDED(hr))
        {
#ifdef CARBONDMP_DEBUG
            DebugOverlay::OnResetDevice();
#endif

            MusicPlayerUI::OnResetDevice();
        }

        return hr;
    }
}


namespace D3DHook
{
    bool Init()
    {
        IDirect3DDevice9* device = nullptr;

        while (!device)
        {
            Sleep(100);

            auto devicePtr =
                reinterpret_cast<IDirect3DDevice9**>(
                    CARBON_DEVICE_PTR
                    );

            if (devicePtr)
                device = *devicePtr;
        }

        if (!device)
        {
            Logger::Error(
                "D3DHook: failed to acquire game device."
            );

            return false;
        }

        void** vtable =
            *reinterpret_cast<void***>(device);

        g_endSceneAddress = vtable[42];
        g_resetAddress = vtable[16];


        MH_STATUS status =
            MH_CreateHook(
                g_endSceneAddress,
                &hkEndScene,
                reinterpret_cast<void**>(&oEndScene)
            );

        if (status != MH_OK)
        {
            Logger::Error(
                "D3DHook: failed to create EndScene hook."
            );

            g_endSceneAddress = nullptr;
            g_resetAddress = nullptr;

            return false;
        }


        status =
            MH_CreateHook(
                g_resetAddress,
                &hkReset,
                reinterpret_cast<void**>(&oReset)
            );

        if (status != MH_OK)
        {
            Logger::Error(
                "D3DHook: failed to create Reset hook."
            );

            MH_RemoveHook(g_endSceneAddress);

            g_endSceneAddress = nullptr;
            g_resetAddress = nullptr;

            return false;
        }


        status =
            MH_EnableHook(
                g_endSceneAddress
            );

        if (status != MH_OK)
        {
            Logger::Error(
                "D3DHook: failed to enable EndScene hook."
            );

            MH_RemoveHook(g_endSceneAddress);
            MH_RemoveHook(g_resetAddress);

            g_endSceneAddress = nullptr;
            g_resetAddress = nullptr;

            return false;
        }


        status =
            MH_EnableHook(
                g_resetAddress
            );

        if (status != MH_OK)
        {
            Logger::Error(
                "D3DHook: failed to enable Reset hook."
            );

            MH_DisableHook(g_endSceneAddress);

            MH_RemoveHook(g_endSceneAddress);
            MH_RemoveHook(g_resetAddress);

            g_endSceneAddress = nullptr;
            g_resetAddress = nullptr;

            return false;
        }

        Logger::Info(
            "D3DHook initialized successfully."
        );

        return true;
    }


    void Shutdown()
    {
        if (g_endSceneAddress)
        {
            MH_DisableHook(
                g_endSceneAddress
            );

            MH_RemoveHook(
                g_endSceneAddress
            );

            g_endSceneAddress = nullptr;
        }

        if (g_resetAddress)
        {
            MH_DisableHook(
                g_resetAddress
            );

            MH_RemoveHook(
                g_resetAddress
            );

            g_resetAddress = nullptr;
        }

        oEndScene = nullptr;
        oReset = nullptr;

        Logger::Info(
            "D3DHook shutdown."
        );
    }
}