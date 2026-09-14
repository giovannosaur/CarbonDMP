#include "pch.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>

#include <cstdio>

#include "debug_overlay.h"

#include "../game/game_state.h"
#include "../game/game_memory.h"
#include "../audio/music_manager.h"
#include "../src/logger.h"

namespace
{
    LPD3DXFONT g_font = nullptr;

    const char* GetGameStateName(GameState state)
    {
        switch (state)
        {
        case GameState::UNKNOWN:
            return "UNKNOWN";

        case GameState::FE:
            return "FE";

        case GameState::GAMEPLAY:
            return "GAMEPLAY";

        case GameState::NIS:
            return "NIS";

        case GameState::LOADING:
            return "LOADING";

        default:
            return "INVALID";
        }
    }

    const char* GetMusicContextName(TrackContext context)
    {
        switch (context)
        {
        case TrackContext::FE:
            return "FE";

        case TrackContext::GAMEPLAY:
            return "GAMEPLAY";

        case TrackContext::BOTH:
            return "BOTH";

        default:
            return "UNKNOWN";
        }
    }

    void CreateFont(LPDIRECT3DDEVICE9 device)
    {
        if (!device)
            return;

        if (g_font)
            return;

        HRESULT hr = D3DXCreateFontA(
            device,
            18,
            0,
            FW_BOLD,
            1,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Arial",
            &g_font
        );

        if (FAILED(hr))
        {
            Logger::Error(
                "DebugOverlay: failed to create D3DX font."
            );
        }
        else
        {
            Logger::Info(
                "DebugOverlay: D3DX font created."
            );
        }
    }

    const char* GetRawGameStateName(int state)
    {
        switch (state)
        {
        case 0:
            return "UNKNOWN";

        case 1:
            return "LOADING_FRONTEND";

        case 2:
            return "UNLOADING_FRONTEND";

        case 3:
            return "IN_FRONTEND";

        case 4:
            return "LOADING_REGION";

        case 5:
            return "LOADING_TRACK";

        case 6:
            return "RACING";

        case 7:
            return "UNLOADING_TRACK";

        case 8:
            return "UNLOADING_REGION";

        case 9:
            return "UNUSED";

        default:
            return "UNKNOWN";
        }
    }
}

namespace DebugOverlay
{
    bool Init()
    {
        Logger::Info(
            "DebugOverlay initialized."
        );

        return true;
    }

    void Render(LPDIRECT3DDEVICE9 device)
    {
        if (!device)
            return;

        CreateFont(device);

        int rawGameState =
            GameMemory::GetGameState();

        if (!g_font)
            return;

        const GameState gameState =
            GameStateManager::GetState();

        const unsigned char menuState =
            GameMemory::GetMenuState();

        const bool paused =
            GameMemory::IsGameplayPaused();

        const Track* currentTrack =
            MusicManager::GetCurrentTrack();

        char buffer[1024];

        if (currentTrack)
        {
            sprintf_s(
                buffer,
                "CarbonDMP DEBUG\n"
                "GameState : %d (%s) -> %s\n"
                "MenuState : %u\n"
                "Paused    : %s\n"
                "Music     : %s - %s (%s)",
                rawGameState,
                GetRawGameStateName(rawGameState),
                GetGameStateName(gameState),
                static_cast<unsigned int>(menuState),
                paused ? "TRUE" : "FALSE",
                currentTrack->artist.c_str(),
                currentTrack->name.c_str(),
                GetMusicContextName(
                    MusicManager::GetContext()
                )
            );
        }
        else
        {
            sprintf_s(
                buffer,
                "CarbonDMP DEBUG\n"
                "GameState : %d (%s) -> %s\n"
                "MenuState : %u\n"
                "Paused    : %s\n"
                "Music     : None",
                rawGameState,
                GetRawGameStateName(rawGameState),
                GetGameStateName(gameState),
                static_cast<unsigned int>(menuState),
                paused ? "TRUE" : "FALSE"
            );
        }

        RECT rect =
        {
            30,
            30,
            900,
            300
        };

        g_font->DrawTextA(
            nullptr,
            buffer,
            -1,
            &rect,
            DT_NOCLIP,
            D3DCOLOR_ARGB(
                255,
                255,
                255,
                255
            )
        );
    }

    void OnLostDevice()
    {
        if (g_font)
            g_font->OnLostDevice();
    }

    void OnResetDevice()
    {
        if (g_font)
            g_font->OnResetDevice();
    }

    void Shutdown()
    {
        if (g_font)
        {
            g_font->Release();
            g_font = nullptr;
        }

        Logger::Info(
            "DebugOverlay shutdown."
        );
    }
}
