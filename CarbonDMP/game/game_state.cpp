#include "pch.h"
#include "game_state.h"
#include "game_memory.h"

#include "../src/logger.h"
#include "../audio/music_manager.h"
#include "../audio/audio_engine.h"
#include "../config/settings.h"
#include <sysinfoapi.h>

namespace
{
    bool g_focusPausedMusic = false;

    GameState g_currentState = GameState::UNKNOWN;
    bool g_wasInNIS = false;
    LoadingContext g_loadingContext = LoadingContext::NONE;
    GameplaySubState g_gameplaySubState = GameplaySubState::NORMAL;

    bool g_modActivated = false;
    int g_lastMenuState = -1;

    // Tracks whether the previous update was inside a blocked FMV.
    bool g_wasInBlockedFMV = false;

    float g_blockedFMVEnterTimer = 0.0f;
    float g_blockedFMVExitTimer = 0.0f;

    constexpr float BLOCKED_FMV_ENTER_DELAY = 0.20f;
    constexpr float BLOCKED_FMV_EXIT_DELAY = 0.30f;

    bool g_fmvPausedMusic = false;
    bool g_musicTransitionPending = false;
}

namespace GameStateManager
{
    float GetGameDeltaTime()
    {
        return *(float*)0xA99A5C;
    }

    void UpdateGameplaySubState();

    namespace
    {
        /* bool IsBlockedFMV()
        {
            const unsigned char menuState =
                GameMemory::GetMenuState();

            const int gameState =
                GameMemory::GetGameState();

            const bool gameplayPaused =
                GameMemory::IsGameplayPaused();

            if (menuState == 83 ||
                menuState == 84 ||
                menuState == 87 ||
                menuState == 217)
            {
                return true;
            }

            if (menuState == 218)
            {
                // Need stronger confirmation while racing.
                if (gameState == 6)
                {
                    return gameplayPaused;
                }

                return true;
            }

            return false;
        } ^^^ IS OLD METHOD */

        bool IsBlockedFMV()
        {
            const bool movieFlagA = (*(unsigned char*)0x00A97A80) != 0;
            const bool movieFlagB = (*(unsigned char*)0x00BCB020) != 0;

            return movieFlagA && movieFlagB;
        }

        bool IsMainMenu()
        {
            return GameMemory::GetMenuState() == 116;
        }

        TrackContext GetFMVResumeContext()
        {
            const unsigned char menuState =
                GameMemory::GetMenuState();

            if (menuState == 116)
            {
                return TrackContext::FE;
            }

            const int gameState =
                GameMemory::GetGameState();

            if (gameState == 3)
            {
                return TrackContext::FE;
            }

            // GAMESTATE 6 = gameplay.
            // Defaulting to GAMEPLAY here is safer than reviving
            // frontend music in an unknown transitional state.
            return TrackContext::GAMEPLAY;
        }

        void UpdateWindowFocus()
        {
            const bool lostFocus =
                GameMemory::HasWindowLostFocus();

            if (lostFocus)
            {
                if (!g_focusPausedMusic &&
                    MusicManager::IsPlaying())
                {
                    Logger::Info(
                        "Window lost focus. Pausing music."
                    );

                    MusicManager::Pause();
                    g_focusPausedMusic = true;
                }

                return;
            }

            if (g_focusPausedMusic)
            {
                if (GameMemory::IsInNIS())
                    return;

                if (IsBlockedFMV())
                    return;

                if (MusicManager::IsPaused())
                {
                    Logger::Info(
                        "Window focus restored. Resuming music."
                    );

                    MusicManager::Resume();
                }

                g_focusPausedMusic = false;
            }
        }

        // -----------------------------------------------------------
        // BLOCKED FMV FILTER (ENTER / EXIT DEBOUNCE)
        // -----------------------------------------------------------
        // Pauses/resumes music around FMVs that must play silent
        // (or with only their own audio). This is intentionally called
        // unconditionally at the very top of Update(), BEFORE the
        // mod-activation gate, the main-menu override, and the NIS
        // handling.
        //
        // It used to live at the very bottom of Update(), after those
        // checks. Any FMV that happened to occur before the mod had
        // been activated (menuState never yet seen as 116), or while
        // GameMemory::IsInNIS() also reported true, hit one of those
        // earlier `return`s first -> this debounce logic was never
        // reached for that tick -> the enter-timer never accumulated
        // -> Pause() never fired. That is what let some blocked
        // MenuStates (e.g. 217) play straight through.
        void UpdateBlockedFMVFilter()
        {
            const bool isBlockedFMV = IsBlockedFMV();
            const float dt = GetGameDeltaTime();

            if (isBlockedFMV)
            {
                // -------------------------------------------------
                // FMV DETECTED
                // -------------------------------------------------

                // While the FMV is active, there is no exit transition
                // to process.
                g_blockedFMVExitTimer = 0.0f;

                // FMV owns music suppression while it is active.
                // Any pending NIS recovery must not start music here.
                g_musicTransitionPending = false;

                if (!g_wasInBlockedFMV)
                {
                    // FMV just appeared.
                    // Require it to remain active for a short period
                    // before pausing music.
                    g_blockedFMVEnterTimer += dt;

                    if (g_blockedFMVEnterTimer >=
                        BLOCKED_FMV_ENTER_DELAY)
                    {
                        Logger::Info(
                            "Blocked FMV detected. Pausing music."
                        );

                        g_fmvPausedMusic =
                            MusicManager::IsPlaying();

                        if (g_fmvPausedMusic)
                        {
                            MusicManager::Pause();
                        }

                        g_wasInBlockedFMV = true;
                        g_blockedFMVEnterTimer = 0.0f;
                    }
                }
                else
                {
                    // FMV is still active.
                    g_blockedFMVEnterTimer = 0.0f;
                }
            }
            else
            {
                // -------------------------------------------------
                // NO FMV DETECTED
                // -------------------------------------------------

                // A non-FMV state cancels any pending FMV-enter
                // confirmation.
                g_blockedFMVEnterTimer = 0.0f;

                if (g_wasInBlockedFMV)
                {
                    // FMV just disappeared.
                    // Require it to remain gone for a short period
                    // before resuming music.
                    g_blockedFMVExitTimer += dt;

                    if (g_blockedFMVExitTimer >=
                        BLOCKED_FMV_EXIT_DELAY)
                    {
                        Logger::Info(
                            "Blocked FMV ended. Recovering music."
                        );

                        if (g_fmvPausedMusic)
                        {
                            // If the FMV ends while a NIS is already active,
                            // do NOT resume music yet.
                            //
                            // The NIS state machine owns the interruption and will
                            // handle music recovery when the NIS actually ends.
                            if (!GameMemory::IsInNIS())
                            {
                                const TrackContext resumeContext =
                                    GetFMVResumeContext();

                                MusicManager::ResumeAfterFMV(
                                    resumeContext
                                );
                            }
                        }

                        g_fmvPausedMusic = false;
                        g_wasInBlockedFMV = false;
                        g_blockedFMVExitTimer = 0.0f;
                    }
                }
                else
                {
                    g_blockedFMVExitTimer = 0.0f;
                }
            }
        }
    }

    void Init()
    {
        g_focusPausedMusic = false;

        g_currentState = GameState::UNKNOWN;
        g_wasInNIS = false;
        g_loadingContext = LoadingContext::NONE;
        g_gameplaySubState = GameplaySubState::NORMAL;

        g_modActivated = false;
        g_lastMenuState = -1;
        g_wasInBlockedFMV = false;
        g_blockedFMVExitTimer = 0.0f;
        g_musicTransitionPending = false;
        g_fmvPausedMusic = false;

        Logger::Info("GameStateManager initialized.");
    }

    void Update()
    {
        UpdateWindowFocus(); // top priority

        const unsigned char menuState = GameMemory::GetMenuState();
        const bool isInNIS = GameMemory::IsInNIS();

        // Must run first, unconditionally, before every early-return
        // branch below (mod-activation gate, main menu override, NIS
        // handling). See UpdateBlockedFMVFilter()'s comment for why.
        UpdateBlockedFMVFilter();

        // ---------------------------------------------------------
        // MAIN MENU / MOD ACTIVATION
        // ---------------------------------------------------------

        if (!g_modActivated && menuState == 116)
        {
            g_modActivated = true;

            Logger::Info(
                "CarbonDMP activated: main menu detected (MenuState 116)."
            );
        }

        // ---------------------------------------------------------
        // BEFORE ACTIVATION
        // ---------------------------------------------------------

        if (!g_modActivated)
        {
            return;
        }

        // ---------------------------------------------------------
        // MAIN MENU OVERRIDE
        // ---------------------------------------------------------
        //
        // MenuState 116 is the actual main menu. Treat it as FE even
        // when the raw game-state value still reports GAMEPLAY during
        // the transition.
        // ---------------------------------------------------------

        if (menuState == 116)
        {
            if (g_currentState != GameState::FE)
            {
                g_loadingContext = LoadingContext::NONE;
                SetState(GameState::FE);
            }

            if (g_musicTransitionPending)
            {
                MusicManager::ResumeAfterInterruption(
                    TrackContext::FE
                );

                g_musicTransitionPending = false;
            }

            UpdateGameplaySubState();

            return;
        }

        // ---------------------------------------------------------
        // NIS
        // ---------------------------------------------------------
        if (isInNIS)
        {
            g_wasInNIS = true;
            SetState(GameState::NIS);
            return;
        }

        if (g_wasInNIS)
        {
            g_wasInNIS = false;

            const int gameState = GameMemory::GetGameState();

            switch (gameState)
            {
            case 3:
                g_loadingContext = LoadingContext::NONE;
                g_musicTransitionPending = true;
                SetState(GameState::FE);
                break;

            case 6:
                g_loadingContext = LoadingContext::NONE;
                g_musicTransitionPending = true;
                SetState(GameState::GAMEPLAY);
                break;

            default:
                SetState(GameState::UNKNOWN);
                break;
            }

            return;
        }

        // ---------------------------------------------------------
        // RAW GAME STATE
        // ---------------------------------------------------------
        const int gameState = GameMemory::GetGameState();

        switch (gameState)
        {
        case 3:
            g_loadingContext = LoadingContext::NONE;
            SetState(GameState::FE);
            break;

        case 6:
            g_loadingContext = LoadingContext::NONE;
            SetState(GameState::GAMEPLAY);
            break;

        case 1:
        case 2:
            g_loadingContext = LoadingContext::FRONTEND;
            SetState(GameState::LOADING);
            break;

        case 4:
        case 5:
        case 7:
        case 8:
            g_loadingContext = LoadingContext::GAMEPLAY;
            SetState(GameState::LOADING);
            break;

        default:
            SetState(GameState::UNKNOWN);
            break;
        }

        UpdateGameplaySubState();
    }

    void UpdateGameplaySubState()
    {
        GameplaySubState newSubState = GameplaySubState::NORMAL;

        if (g_currentState == GameState::GAMEPLAY &&
            GameMemory::IsGameplayPaused())
        {
            newSubState = GameplaySubState::PAUSED;
        }

        if (g_gameplaySubState == newSubState)
            return;

        g_gameplaySubState = newSubState;

        if (newSubState == GameplaySubState::PAUSED)
        {
            Logger::Info("Gameplay substate changed: PAUSED");

            AudioEngine::SetLowPassEnabled(
                Settings::IsPausedLPFEnabled()
            );
        }
        else
        {
            Logger::Info("Gameplay substate changed: NORMAL");

            AudioEngine::SetLowPassEnabled(false);
        }
    }

    GameState GetState()
    {
        return g_currentState;
    }

    GameplaySubState GetGameplaySubState()
    {
        return g_gameplaySubState;
    }

    LoadingContext GetLoadingContext()
    {
        return g_loadingContext;
    }

    void SetState(GameState state)
    {
        const bool stateChanged =
            (g_currentState != state);

        const bool loadingContextChanged =
            (state == GameState::LOADING &&
                g_currentState == GameState::LOADING);

        if (!stateChanged && !loadingContextChanged)
            return;

        g_currentState = state;

        switch (state)
        {
        case GameState::UNKNOWN:
            Logger::Info("Game state changed: UNKNOWN");
            break;

        case GameState::FE:
        {
            Logger::Info("Game state changed: FE");

            AudioEngine::SetFEReverbEnabled(
                Settings::IsFEReverbEnabled()
            );

            AudioEngine::SetFEEQEnabled(
                Settings::IsFEEQEnabled()
            );

            break;
        }

        case GameState::GAMEPLAY:
        {
            Logger::Info("Game state changed: GAMEPLAY");

            AudioEngine::SetFEReverbEnabled(false);
            AudioEngine::SetFEEQEnabled(false);

            break;
        }

        case GameState::NIS:
            Logger::Info("Game state changed: NIS");

            AudioEngine::SetFEReverbEnabled(false);
            AudioEngine::SetFEEQEnabled(false);

            break;

        case GameState::LOADING:
            if (g_loadingContext == LoadingContext::FRONTEND)
                Logger::Info("Game state changed: LOADING (FRONTEND)");
            else if (g_loadingContext == LoadingContext::GAMEPLAY)
                Logger::Info("Game state changed: LOADING (GAMEPLAY)");
            else
                Logger::Info("Game state changed: LOADING");

            AudioEngine::SetFEReverbEnabled(false);
            AudioEngine::SetFEEQEnabled(false);

            break;
        }

        // Single source of truth for what the music should be doing in
        // each state:
        //   FE / GAMEPLAY -> make sure the right context is playing.
        //   NIS / LOADING -> fully stop (previously this never actually
        //                    ran for these two states, which is why
        //                    music kept playing through loading screens,
        //                    NIS cutscenes, and the FMV-triggering menus).
        //   UNKNOWN       -> leave whatever is currently happening alone.
        HandleMusicState();
    }

    void HandleMusicState()
    {
        switch (g_currentState)
        {
        case GameState::FE:
            if (g_wasInBlockedFMV)
                break;

            if (g_musicTransitionPending)
            {
                MusicManager::ResumeAfterInterruption(
                    TrackContext::FE
                );

                g_musicTransitionPending = false;
                break;
            }

            MusicManager::SetContext(
                TrackContext::FE
            );

            if (!MusicManager::IsPlaying() &&
                !MusicManager::IsPaused())
            {
                MusicManager::Play();
            }

            break;

        case GameState::GAMEPLAY:
            if (g_wasInBlockedFMV)
                break;

            if (g_musicTransitionPending)
            {
                MusicManager::ResumeAfterInterruption(
                    TrackContext::GAMEPLAY
                );

                g_musicTransitionPending = false;
                break;
            }

            MusicManager::SetContext(
                TrackContext::GAMEPLAY
            );

            if (!MusicManager::IsPlaying() &&
                !MusicManager::IsPaused())
            {
                MusicManager::Play();
            }

            break;

        case GameState::NIS:
            MusicManager::Stop();
            break;

        case GameState::LOADING:
            // Both frontend and gameplay loading must silence
            // the custom music.
            MusicManager::Stop();
            break;

        case GameState::UNKNOWN:
            // Do not start music from an unknown state.
            break;
        }
    }
}