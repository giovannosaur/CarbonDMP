#pragma once

#include "track.h"

namespace MusicManager
{
    bool Init();
    void Shutdown();

    void Play();
    void Stop();
    void Pause();
    void Resume();

    void Next();
    void Previous();

    void ProcessTrackEnd();

    bool IsPlaying();
    bool IsPaused();

    void TransitionToContext(TrackContext context);
    void ResumeAfterInterruption(TrackContext context);
    void ResumeAfterFMV(TrackContext context);

    const Track* GetCurrentTrack();

    void PlayTrack(size_t trackIndex);
    void RefreshPlaylist();

    void SetContext(TrackContext context);
    TrackContext GetContext();
    size_t GetCurrentIndex();

    void ToggleShuffle();
    bool IsShuffleEnabled();

    void GetCurrentPlaylistPosition(
        size_t& position,
        size_t& total
    );
}
