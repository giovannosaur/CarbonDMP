#pragma once

#include <string>
#include <functional>

namespace AudioEngine
{
    using TrackEndCallback = std::function<void()>;

    bool Init();
    void Shutdown();

    bool Play(const std::string& filename);
    void Stop();
    void Pause();
    void Resume();

    bool IsPlaying();
    bool IsPaused();

    void SetVolume(float volume);

    void ToggleMute();
    bool IsMuted();

    void SetLowPassEnabled(bool enabled);
    bool IsLowPassEnabled();

    void SetLowPassFrequency(float frequency);
    float GetLowPassFrequency();

    void SetLowPassQ(float q);
    float GetLowPassQ();

    // ---------------------------------------------------------
    // FRONTEND ROOM EFFECT
    // ---------------------------------------------------------

    void SetFEReverbEnabled(bool enabled);
    bool IsFEReverbEnabled();

    void SetFEEQEnabled(bool enabled);
    bool IsFEEQEnabled();

    void SetFEEffectMix(float mix);
    float GetFEEffectMix();

    // ---------------------------------------------------------
    // DYNAMIC AUDIO EFFECT
    // ---------------------------------------------------------

    void SetDynamicVolumeMultiplier(float multiplier);
    float GetDynamicVolumeMultiplier();

    void SetDynamicLowPassFrequency(float frequency);
    float GetDynamicLowPassFrequency();

    void SetDynamicLowPassEnabled(bool enabled);
    bool IsDynamicLowPassEnabled();

    void SetIdleEffectAmount(float amount);

    void SetSpeedbreakerEffectAmount(float amount);

    void ApplyDynamicAudioState(
        float volumeMultiplier,
        float lpfFrequency,
        bool lpfRequested,
        float idleEffectAmount,
        float speedbreakerEffectAmount
    );

    void SetTrackEndCallback(TrackEndCallback callback);
}