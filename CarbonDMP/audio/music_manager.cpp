#include "pch.h"

// Windows headers may define max/min as macros.
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "music_manager.h"

#include "audio_engine.h"
#include "playlist.h"

#include "../src/logger.h"
#include "../config/paths.h"

#include <vector>
#include <limits>
#include <random>
#include <atomic>
#include <mutex>

namespace
{
    // Protects g_activePlaylist / g_currentIndex / g_currentTrackIndex /
    // g_currentContext, and every function below that reads or mutates
    // them (Play, PlayTrack, Next, Previous, SetContext, RefreshPlaylist,
    // ToggleShuffle, ProcessTrackEnd).
    //
    // These can be triggered from two different threads: the worker
    // thread (a track naturally ending -> ProcessTrackEnd -> Next) and
    // the D3D9 render thread (manual Next/Previous/PlayTrack from the
    // in-game UI). Recursive because some of these functions call each
    // other (e.g. Previous() -> Play(), StartCurrentTrack() -> PlayGlobalTrack()).
    std::recursive_mutex g_stateMutex;

    std::vector<size_t> g_activePlaylist;
    size_t g_currentIndex = 0;

    // Global index in Playlist::GetTracks().
    // This lets the Music tab play any track, even if it is
    // outside the current game context.
    size_t g_currentTrackIndex =
        (std::numeric_limits<size_t>::max)();

    TrackContext g_currentContext =
        TrackContext::GAMEPLAY;

    bool g_initialized = false;
    bool g_shuffleEnabled = false;
    std::atomic<bool> g_trackEnded = false;

    size_t g_lastFETrackIndex =
        (std::numeric_limits<size_t>::max)();

    size_t g_lastGameplayTrackIndex =
        (std::numeric_limits<size_t>::max)();
}

namespace MusicManager
{
    void Next();
}

namespace
{
    void SetPlaylistForContext(TrackContext context)
    {
        g_activePlaylist =
            Playlist::GetTracksForContext(context);

        g_currentIndex = 0;
        g_currentContext = context;

        if (g_shuffleEnabled && g_activePlaylist.size() > 1)
        {
            std::random_device rd;
            std::mt19937 generator(rd());

            std::shuffle(
                g_activePlaylist.begin(),
                g_activePlaylist.end(),
                generator
            );
        }

        if (!g_activePlaylist.empty())
        {
            g_currentTrackIndex =
                g_activePlaylist[0];
        }
        else
        {
            g_currentTrackIndex =
                (std::numeric_limits<size_t>::max)();
        }
    }

    bool FindActiveTrack(
        size_t trackIndex,
        size_t& activeIndex
    )
    {
        for (size_t i = 0;
            i < g_activePlaylist.size();
            ++i)
        {
            if (g_activePlaylist[i] == trackIndex)
            {
                activeIndex = i;
                return true;
            }
        }

        return false;
    }

    size_t& GetLastTrackIndexForContext(
        TrackContext context
    )
    {
        if (context == TrackContext::FE)
            return g_lastFETrackIndex;

        return g_lastGameplayTrackIndex;
    }

    void StartCurrentTrack();

    void AdvanceToNextTrackLocked()
    {
        if (!g_initialized)
            return;

        if (g_activePlaylist.empty())
            return;

        size_t activeIndex = 0;

        if (FindActiveTrack(
            g_currentTrackIndex,
            activeIndex))
        {
            g_currentIndex = activeIndex + 1;

            if (g_currentIndex >=
                g_activePlaylist.size())
            {
                g_currentIndex = 0;
            }
        }
        else
        {
            // Current track is not part of this context.
            // Continue to the first track after it in the
            // active playlist's track ordering.
            g_currentIndex = 0;

            for (size_t i = 0;
                i < g_activePlaylist.size();
                ++i)
            {
                if (g_activePlaylist[i] >
                    g_currentTrackIndex)
                {
                    g_currentIndex = i;
                    break;
                }
            }
        }

        StartCurrentTrack();
    }

    void StartNextTrackForContextLocked(
        TrackContext context
    )
    {
        if (!g_initialized)
            return;

        if (g_activePlaylist.empty())
            return;

        size_t& lastTrackIndex =
            GetLastTrackIndexForContext(context);

        // This context has never played a track before.
        // Start from the first track in its playlist.
        if (lastTrackIndex ==
            (std::numeric_limits<size_t>::max)())
        {
            g_currentIndex = 0;
            StartCurrentTrack();
            return;
        }

        size_t activeIndex = 0;

        // The last track is still part of this context.
        if (FindActiveTrack(
            lastTrackIndex,
            activeIndex))
        {
            g_currentIndex = activeIndex + 1;

            if (g_currentIndex >=
                g_activePlaylist.size())
            {
                g_currentIndex = 0;
            }

            StartCurrentTrack();
            return;
        }

        // Last track is no longer part of this context.
        // Find the first active track after it.
        g_currentIndex = 0;

        for (size_t i = 0;
            i < g_activePlaylist.size();
            ++i)
        {
            if (g_activePlaylist[i] >
                lastTrackIndex)
            {
                g_currentIndex = i;
                break;
            }
        }

        StartCurrentTrack();
    }

    void PlayGlobalTrack(size_t trackIndex)
    {
        const auto& tracks =
            Playlist::GetTracks();

        if (trackIndex >= tracks.size())
            return;

        const Track& track =
            tracks[trackIndex];

        const std::string musicPath =
            Paths::GetMusicFile(track.filename);

        Logger::Info(
            ("MusicManager playing: " +
                track.name).c_str()
        );

        g_currentTrackIndex = trackIndex;

        size_t activeIndex = 0;

        if (FindActiveTrack(
            trackIndex,
            activeIndex))
        {
            g_currentIndex = activeIndex;

            // This track belongs to the current context,
            // so it becomes that context's latest played track.
            GetLastTrackIndexForContext(
                g_currentContext
            ) = trackIndex;
        }

        if (!AudioEngine::Play(musicPath))
        {
            Logger::Error(
                "MusicManager failed to play track."
            );
        }
    }

    void StartCurrentTrack()
    {
        if (g_activePlaylist.empty())
            return;

        if (g_currentIndex >=
            g_activePlaylist.size())
        {
            g_currentIndex = 0;
        }

        const size_t trackIndex =
            g_activePlaylist[g_currentIndex];

        PlayGlobalTrack(trackIndex);
    }
}

namespace MusicManager
{
    void TransitionToContext(TrackContext context)
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        // This function is only for an actual context transition.
        // If we're already in the requested context, do nothing.
        if (g_currentContext == context)
            return;

        g_trackEnded.store(false);

        SetPlaylistForContext(context);

        if (g_activePlaylist.empty())
        {
            AudioEngine::Stop();
            return;
        }

        Logger::Info(
            context == TrackContext::FE
            ? "MusicManager transition: FE"
            : "MusicManager transition: GAMEPLAY"
        );

        StartNextTrackForContextLocked(context);
    }

    void ResumeAfterInterruption(TrackContext context)
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        g_trackEnded.store(false);

        // The interruption may have ended in a different context.
        // In that case, perform a normal context transition.
        if (g_currentContext != context)
        {
            SetPlaylistForContext(context);

            if (g_activePlaylist.empty())
            {
                AudioEngine::Stop();
                return;
            }

            Logger::Info(
                context == TrackContext::FE
                ? "MusicManager interruption recovery: FE"
                : "MusicManager interruption recovery: GAMEPLAY"
            );

            StartNextTrackForContextLocked(context);
            return;
        }

        // Same context as before the interruption.
        // The current track was stopped/paused for the interruption,
        // so continue with the NEXT track instead of restarting it.
        if (g_activePlaylist.empty())
            return;

        Logger::Info(
            context == TrackContext::FE
            ? "MusicManager interruption recovery: FE -> next track"
            : "MusicManager interruption recovery: GAMEPLAY -> next track"
        );

        AdvanceToNextTrackLocked();
    }

    void ResumeAfterFMV(TrackContext context)
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        g_trackEnded.store(false);

        // FMV ended in a different context than the one
        // MusicManager was using before the FMV.
        if (g_currentContext != context)
        {
            SetPlaylistForContext(context);

            if (g_activePlaylist.empty())
            {
                AudioEngine::Stop();
                return;
            }

            Logger::Info(
                context == TrackContext::FE
                ? "MusicManager FMV recovery: FE"
                : "MusicManager FMV recovery: GAMEPLAY"
            );

            StartNextTrackForContextLocked(context);
            return;
        }

        // Same context: FMV only paused the current music.
        Logger::Info(
            context == TrackContext::FE
            ? "MusicManager FMV recovery: resuming FE music"
            : "MusicManager FMV recovery: resuming GAMEPLAY music"
        );

        if (AudioEngine::IsPaused())
        {
            AudioEngine::Resume();
            return;
        }

        // Safety fallback in case the stream is no longer paused.
        if (!AudioEngine::IsPlaying())
        {
            StartCurrentTrack();
        }
    }

    bool IsPlaying()
    {
        return AudioEngine::IsPlaying();
    }

    bool IsPaused()
    {
        return AudioEngine::IsPaused();
    }

    bool Init()
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        SetPlaylistForContext(
            TrackContext::GAMEPLAY
        );

        if (g_activePlaylist.empty())
        {
            Logger::Error(
                "MusicManager: no tracks available for current context."
            );

            return false;
        }

        g_currentIndex = 0;
        g_currentTrackIndex =
            g_activePlaylist[0];

        g_lastFETrackIndex =
            (std::numeric_limits<size_t>::max)();

        g_lastGameplayTrackIndex =
            (std::numeric_limits<size_t>::max)();

        g_initialized = true;

        g_trackEnded.store(false);

        AudioEngine::SetTrackEndCallback(
            []()
            {
                g_trackEnded.store(true);
            }
        );

        Logger::Info(
            "MusicManager initialized."
        );

        return true;
    }

    void Shutdown()
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        AudioEngine::SetTrackEndCallback(
            nullptr
        );

        g_trackEnded.store(false);

        g_initialized = false;

        g_activePlaylist.clear();

        g_currentIndex = 0;

        g_currentTrackIndex =
            (std::numeric_limits<size_t>::max)();

        g_lastFETrackIndex =
            (std::numeric_limits<size_t>::max)();

        g_lastGameplayTrackIndex =
            (std::numeric_limits<size_t>::max)();

        Logger::Info(
            "MusicManager shutdown."
        );
    }

    void Play()
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        if (g_activePlaylist.empty())
            return;

        // A natural track-end is waiting to be processed.
        // Never restart the current track while that event is pending.
        if (g_trackEnded.load())
            return;

        if (AudioEngine::IsPlaying())
            return;

        if (AudioEngine::IsPaused())
        {
            AudioEngine::Resume();
            return;
        }

        StartCurrentTrack();
    }

    void PlayTrack(size_t trackIndex)
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        const auto& tracks =
            Playlist::GetTracks();

        if (trackIndex >= tracks.size())
        {
            Logger::Error(
                "MusicManager: invalid PlayTrack index."
            );

            return;
        }

        // Manual selection supersedes a pending natural-end event.
        g_trackEnded.store(false);

        PlayGlobalTrack(trackIndex);
    }

    void RefreshPlaylist()
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        const bool wasPlaying =
            AudioEngine::IsPlaying();

        const size_t oldTrackIndex =
            g_currentTrackIndex;

        SetPlaylistForContext(
            g_currentContext
        );

        if (g_activePlaylist.empty())
        {
            AudioEngine::Stop();

            Logger::Error(
                "MusicManager: playlist became empty after refresh."
            );

            return;
        }

        // Keep the currently selected/playing global track.
        // It is allowed to exist outside the active context because
        // the Music tab can manually play any track.
        g_currentTrackIndex = oldTrackIndex;

        size_t activeIndex = 0;

        if (FindActiveTrack(
            g_currentTrackIndex,
            activeIndex))
        {
            g_currentIndex = activeIndex;
        }
        else
        {
            // Current track is no longer part of the active context.
            // Keep playing it. Next/Previous will resolve the next
            // active track when needed.
            g_currentIndex = 0;
        }

        // IMPORTANT:
        // Do not call Play() here.
        //
        // Refreshing the playlist must only change playlist membership.
        // It must never unexpectedly replace the track currently
        // playing in the audio engine.
        (void)wasPlaying;
    }

    void Stop()
    {
        AudioEngine::Stop();
    }

    void Pause()
    {
        AudioEngine::Pause();
    }

    void Resume()
    {
        AudioEngine::Resume();
    }

    void Next()
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        g_trackEnded.store(false);

        AdvanceToNextTrackLocked();
    }

    void Previous()
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        // Manual selection supersedes a pending natural-end event.
        g_trackEnded.store(false);

        if (!g_initialized)
            return;

        if (g_activePlaylist.empty())
            return;

        size_t activeIndex = 0;

        if (FindActiveTrack(
            g_currentTrackIndex,
            activeIndex))
        {
            g_currentIndex =
                activeIndex;
        }
        else
        {
            // If the Music tab manually played a track
            // outside the active context, continue with
            // the last active track before it.
            for (size_t i =
                g_activePlaylist.size();
                i > 0;
                --i)
            {
                const size_t index = i - 1;

                if (g_activePlaylist[index] <
                    g_currentTrackIndex)
                {
                    g_currentIndex = index;
                    StartCurrentTrack();
                    return;
                }
            }

            // Nothing comes before the manually selected
            // track, so wrap to the last active track.
            g_currentIndex =
                g_activePlaylist.size() - 1;

            StartCurrentTrack();
            return;
        }

        if (g_currentIndex == 0)
        {
            g_currentIndex =
                g_activePlaylist.size() - 1;
        }
        else
        {
            g_currentIndex--;
        }

        StartCurrentTrack();
    }

    void ProcessTrackEnd()
    {
        if (!g_trackEnded.exchange(false))
            return;

        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        Logger::Info(
            "MusicManager: processing track-end event."
        );

        Next();
    }

    void SetContext(TrackContext context)
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        // Already in this context.
        // Do absolutely nothing. The current track must continue playing.
        if (g_currentContext == context)
            return;

        SetPlaylistForContext(context);

        if (g_activePlaylist.empty())
        {
            AudioEngine::Stop();
            return;
        }

        g_trackEnded.store(false);

        Logger::Info(
            context == TrackContext::FE
            ? "MusicManager context changed: FE"
            : "MusicManager context changed: GAMEPLAY"
        );

        StartNextTrackForContextLocked(context);
    }

    TrackContext GetContext()
    {
        return g_currentContext;
    }

    size_t GetCurrentIndex()
    {
        return g_currentIndex;
    }

    void GetCurrentPlaylistPosition(
        size_t& position,
        size_t& total)
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_stateMutex
        );

        position = 0;
        total =
            g_activePlaylist.size();

        if (!g_initialized ||
            g_activePlaylist.empty())
        {
            return;
        }

        size_t activeIndex = 0;

        if (!FindActiveTrack(
            g_currentTrackIndex,
            activeIndex))
        {
            return;
        }

        position =
            activeIndex + 1;
    }

    void ToggleShuffle()
    {
        std::lock_guard<std::recursive_mutex> lock(g_stateMutex);

        if (!g_initialized)
            return;

        const size_t oldTrackIndex =
            g_currentTrackIndex;

        g_shuffleEnabled =
            !g_shuffleEnabled;

        SetPlaylistForContext(
            g_currentContext
        );

        if (g_activePlaylist.empty())
        {
            AudioEngine::Stop();
            return;
        }

        size_t activeIndex = 0;

        if (oldTrackIndex !=
            (std::numeric_limits<size_t>::max)() &&
            FindActiveTrack(
                oldTrackIndex,
                activeIndex))
        {
            g_currentIndex = activeIndex;
            g_currentTrackIndex = oldTrackIndex;
        }
        else
        {
            g_currentIndex = 0;
            g_currentTrackIndex =
                g_activePlaylist[0];
        }

        Logger::Info(
            g_shuffleEnabled
            ? "MusicManager: shuffle ON."
            : "MusicManager: shuffle OFF."
        );
    }

    bool IsShuffleEnabled()
    {
        return g_shuffleEnabled;
    }

    const Track* GetCurrentTrack()
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_stateMutex
        );

        static bool lastInitialized = false;
        static size_t lastTrackIndex = (std::numeric_limits<size_t>::max)();
        static size_t lastTrackCount = (std::numeric_limits<size_t>::max)();

        const auto& tracks = Playlist::GetTracks();

        const bool stateChanged =
            (g_initialized != lastInitialized) ||
            (g_currentTrackIndex != lastTrackIndex) ||
            (tracks.size() != lastTrackCount);

        if (stateChanged)
        {
            char buffer[256];

            sprintf_s(
                buffer,
                "GetCurrentTrack state: initialized=%d index=%zu tracks=%zu",
                g_initialized ? 1 : 0,
                g_currentTrackIndex,
                tracks.size()
            );

            Logger::Info(buffer);

            if (g_initialized &&
                g_currentTrackIndex < tracks.size())
            {
                const Track& track = tracks[g_currentTrackIndex];

                Logger::Info(
                    ("GetCurrentTrack metadata: name=" +
                        track.name +
                        " | artist=" +
                        track.artist +
                        " | album=" +
                        track.album).c_str()
                );
            }

            lastInitialized = g_initialized;
            lastTrackIndex = g_currentTrackIndex;
            lastTrackCount = tracks.size();
        }

        if (!g_initialized)
            return nullptr;

        if (g_currentTrackIndex >= tracks.size())
            return nullptr;

        return &tracks[g_currentTrackIndex];
    }
}
