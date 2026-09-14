#include "pch.h"

#include <windows.h>

#include "src/logger.h"

#include "audio/audio_engine.h"
#include "audio/dynamic_audio.h"
#include "audio/playlist.h"
#include "audio/music_manager.h"

#include "config/paths.h"
#include "config/settings.h"

#include "d3d/d3d_hook.h"

#include "game/game_state.h"
#include "game/hooks.h"

#include "ui/debug_overlay.h"
#include "ui/music_player_ui.h"


DWORD WINAPI CarbonDMPThread(LPVOID)
{
    Logger::Init();
    GameStateManager::Init();

    Logger::Info("CarbonDMP thread started.");

    if (!Paths::Init())
    {
        Logger::Error("Failed to initialize mod paths.");
        return 0;
    }

    Logger::Info(Paths::GetModRoot().c_str());

    if (!Settings::Init())
    {
        Logger::Error("Settings initialization failed.");
        return 0;
    }

    if (!AudioEngine::Init())
    {
        Logger::Error("Audio engine initialization failed.");
        return 0;
    }

    if (!Playlist::LoadFromFile(Paths::GetDataFile()))
    {
        Logger::Error("Playlist loading failed.");
        AudioEngine::Shutdown();
        return 0;
    }

    const auto& tracks = Playlist::GetTracks();

    char buffer[256];

    sprintf_s(
        buffer,
        "Loaded %zu tracks from data.dat.",
        tracks.size()
    );

    Logger::Info(buffer);

    if (!MusicManager::Init())
    {
        Logger::Error("MusicManager initialization failed.");
        AudioEngine::Shutdown();
        return 0;
    }

    if (!GameHooks::Init())
    {
        Logger::Error("GameHooks initialization failed.");
    }

    if (!DynamicAudio::Init())
    {
        Logger::Error(
            "DynamicAudio initialization failed."
        );
    }

    Sleep(1000);

    if (!MusicPlayerUI::Init())
    {
        Logger::Error(
            "MusicPlayerUI initialization failed."
        );
    }

#ifdef CARBONDMP_DEBUG
    if (!DebugOverlay::Init())
    {
        Logger::Error(
            "DebugOverlay initialization failed."
        );
    }
#endif

    if (!D3DHook::Init())
    {
        Logger::Error(
            "D3DHook initialization failed."
        );
    }

    while (true)
    {
        GameStateManager::Update();

        DynamicAudio::Update(
            GameStateManager::GetGameDeltaTime()
        );

        // NOTE: MusicPlayerUI::Update() is intentionally NOT called here.
        // It is already called once per rendered frame from hkEndScene()
        // in debug_overlay.cpp, which is the correct single place for it
        // (it needs to run in lockstep with Render() on the D3D9 thread).
        //
        // Calling it here too caused it to run concurrently on two
        // different threads (this thread + the D3D9 render thread) with
        // no synchronization, which is what caused overlapping/stacked
        // playback when using Next/Previous or manually selecting a
        // track from the tracklist.

        MusicManager::ProcessTrackEnd();

        Sleep(16);
    }
}

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved
)
    {
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        CreateThread(
            nullptr,
            0,
            CarbonDMPThread,
            nullptr,
            0,
            nullptr
        );

        break;

    case DLL_PROCESS_DETACH:

        D3DHook::Shutdown();

        #ifdef CARBONDMP_DEBUG
        DebugOverlay::Shutdown();
        #endif

        MusicPlayerUI::Shutdown();
        DynamicAudio::Shutdown();
        GameHooks::Shutdown();
        MusicManager::Shutdown();
        AudioEngine::Shutdown();
        Logger::Shutdown();

        break;
    }
    return TRUE;
}