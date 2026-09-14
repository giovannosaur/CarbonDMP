#include "pch.h"

#include "audio_engine.h"

#include <bass.h>
#include <bass_fx.h>

#include "../src/logger.h"
#include "../config/settings.h"

#include <functional>
#include <mutex>

namespace
{
    // Protects every operation below that touches g_currentStream or any
    // of the FX handles (g_lowPassFX / g_feReverbFX / g_feEQFX).
    //
    // Play()/Stop()/Pause()/Resume() can legitimately be called from two
    // different threads: the mod's own worker thread (e.g. a track
    // naturally ending -> MusicManager::ProcessTrackEnd() -> Next()) and
    // the game's D3D9 render thread (manual Next/Previous/PlayTrack from
    // the in-game UI). Without this lock, "Stop old stream, then create
    // and start a new one" is not atomic across those two threads, which
    // is what allowed two BASS streams to end up playing at once.
    std::recursive_mutex g_audioMutex;

    HSTREAM g_currentStream = 0;
    HFX g_lowPassFX = 0;
    HFX g_feReverbFX = 0;
    HFX g_feEQFX = 0;
    HFX g_idleEQFX = 0;
    HFX g_idleReverbFX = 0;
    HDSP g_idleStereoWidthDSP = 0;

    float g_volume = 1.0f;
    float g_volumeBeforeMute = 1.0f;
    bool g_muted = false;

    // Desired LPF state from the game-state system.
    bool g_lowPassRequested = false;

    // Actual LPF state currently applied to the active stream.
    bool g_lowPassEnabled = false;
    float g_lowPassFrequency = 700.0f;
    float g_lowPassQ = 0.707f;

    // Dynamic/Speed-sensitive LPF
    float g_dynamicLowPassFrequency = 20000.0f;
    constexpr float DYNAMIC_LOW_PASS_Q = 0.5400f;

    // Idle Effect Chain
    float g_idleEffectAmount = 0.0f;
    float g_idleMonoAmount = 0.0f;
    constexpr float IDLE_MONO_AMOUNT_MAX = 0.70f;
    constexpr float IDLE_LOW_PASS_CUTOFF = 500.0f;
    constexpr float IDLE_LOW_PASS_Q = 0.300f;
    constexpr float IDLE_LOW_BOOST_CENTER = 100.0f;
    constexpr float IDLE_LOW_BOOST_MAX_GAIN = 4.0f;
    constexpr float IDLE_LOW_BOOST_BANDWIDTH = 0.8f;

    // Speedbreaker LPF
    float g_speedbreakerEffectAmount = 0.0f;
    constexpr float SPEEDBREAKER_LOW_PASS_CUTOFF = 1800.0f;
    constexpr float SPEEDBREAKER_LOW_PASS_Q = 0.5400f;

    // Desired FE room effect state from the game-state system.
    bool g_feReverbRequested = false;
    bool g_feEQRequested = false;

    // Actual FE room effect state currently applied.
    bool g_feReverbEnabled = false;
    bool g_feEQEnabled = false;

    float g_feEffectMix = 25.0f;

    // dynamic driving audio
    float g_dynamicVolumeMultiplier = 1.0f;
    bool g_dynamicLowPassRequested = false;

    struct DynamicAudioState
    {
        float volumeMultiplier = 1.0f;
        float lpfFrequency = 20000.0f;
        bool lpfRequested = false;
        float idleEffectAmount = 0.0f;
        float speedbreakerEffectAmount = 0.0f;
    };

    float GetFinalVolume()
    {
        if (g_muted)
            return 0.0f;

        return g_volume *
            g_dynamicVolumeMultiplier;
    }

    void CALLBACK IdleStereoWidthDSP(
        HDSP handle,
        DWORD channel,
        void* buffer,
        DWORD length,
        void* user
    )
    {
        if (!buffer)
            return;

        const float monoAmount =
            g_idleMonoAmount;

        if (monoAmount <= 0.0f)
            return;

        const int sampleCount =
            static_cast<int>(length / sizeof(float));

        float* samples =
            static_cast<float*>(buffer);

        for (int i = 0; i + 1 < sampleCount; i += 2)
        {
            const float left =
                samples[i];

            const float right =
                samples[i + 1];

            const float mid =
                (left + right) * 0.5f;

            const float side =
                (left - right) * 0.5f;

            const float reducedSide =
                side * (1.0f - monoAmount);

            samples[i] =
                mid + reducedSide;

            samples[i + 1] =
                mid - reducedSide;
        }
    }

    float GetEffectiveLowPassFrequency()
    {
        // 1. Pause/system LPF always wins.
        if (g_lowPassRequested)
            return g_lowPassFrequency;

        // 2. Start from the normal dynamic LPF.
        float cutoff =
            g_dynamicLowPassFrequency;

        // 3. IAOI modifies the normal dynamic LPF.
        if (g_idleEffectAmount > 0.0f)
        {
            cutoff =
                cutoff +
                (
                    (IDLE_LOW_PASS_CUTOFF - cutoff) *
                    g_idleEffectAmount
                    );
        }

        // 4. Speedbreaker overlays the resulting cutoff.
        if (g_speedbreakerEffectAmount > 0.0f)
        {
            cutoff =
                cutoff +
                (
                    (SPEEDBREAKER_LOW_PASS_CUTOFF - cutoff) *
                    g_speedbreakerEffectAmount
                    );
        }

        return cutoff;
    }

    float GetEffectiveLowPassQ()
    {
        if (g_lowPassRequested)
            return g_lowPassQ;

        if (g_speedbreakerEffectAmount > 0.0f)
            return SPEEDBREAKER_LOW_PASS_Q;

        if (g_idleEffectAmount > 0.0f)
            return IDLE_LOW_PASS_Q;

        return DYNAMIC_LOW_PASS_Q;
    }

    bool ShouldLowPassBeActive()
    {
        return
            g_lowPassRequested ||
            g_dynamicLowPassRequested;
    }

    void ApplyEffectiveLowPassFrequency()
    {
        if (!g_lowPassFX)
            return;

        BASS_BFX_BQF lowPass = {};

        if (!BASS_FXGetParameters(
            g_lowPassFX,
            &lowPass
        ))
        {
            Logger::Error(
                "Failed to get BASS low-pass parameters."
            );
            return;
        }

        lowPass.fCenter =
            GetEffectiveLowPassFrequency();
        lowPass.fQ =
            GetEffectiveLowPassQ();

        if (!BASS_FXSetParameters(
            g_lowPassFX,
            &lowPass
        ))
        {
            Logger::Error(
                "Failed to apply effective BASS low-pass frequency."
            );
        }
    }

    void ApplyIdleEQ()
    {
        if (!g_idleEQFX)
            return;

        BASS_BFX_PEAKEQ eq = {};

        if (!BASS_FXGetParameters(
            g_idleEQFX,
            &eq
        ))
        {
            Logger::Error(
                "Failed to get IAOI EQ parameters."
            );
            return;
        }

        eq.lBand = 0;
        eq.fCenter = IDLE_LOW_BOOST_CENTER;
        eq.fGain =
            IDLE_LOW_BOOST_MAX_GAIN *
            g_idleEffectAmount;
        eq.fBandwidth = IDLE_LOW_BOOST_BANDWIDTH;
        eq.fQ = 0.0f;
        eq.lChannel = BASS_BFX_CHANALL;

        if (!BASS_FXSetParameters(
            g_idleEQFX,
            &eq
        ))
        {
            Logger::Error(
                "Failed to apply IAOI EQ parameters."
            );
        }
    }

    void ApplyIdleReverb()
    {
        if (!g_idleReverbFX)
            return;

        BASS_BFX_FREEVERB reverb = {};

        if (!BASS_FXGetParameters(
            g_idleReverbFX,
            &reverb
        ))
        {
            Logger::Error(
                "Failed to get IAOI reverb parameters."
            );
            return;
        }

        reverb.fDryMix = 0.85f;
        reverb.fWetMix = 0.15f * g_idleEffectAmount;
        reverb.fRoomSize = 0.15f;
        reverb.fDamp = 0.60f;
        reverb.fWidth = 0.70f;
        reverb.lMode = 0;
        reverb.lChannel = BASS_BFX_CHANALL;

        if (!BASS_FXSetParameters(
            g_idleReverbFX,
            &reverb
        ))
        {
            Logger::Error(
                "Failed to apply IAOI reverb parameters."
            );
        }
    }

    AudioEngine::TrackEndCallback g_trackEndCallback;

    unsigned int g_trackEndCount = 0;
}

void CALLBACK TrackEndSync(
    HSYNC handle,
    DWORD channel,
    DWORD data,
    void* user
)
{
    ++g_trackEndCount;

    AudioEngine::TrackEndCallback callback;

    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        if (channel != g_currentStream)
        {
            Logger::Info(
                "TrackEndSync ignored: callback belongs to old/non-current stream."
            );

            return;
        }

        callback = g_trackEndCallback;
    }

    if (callback)
    {
        Logger::Info(
            "TrackEndSync: calling track-end callback."
        );

        callback();
    }
}

namespace AudioEngine
{
    void ApplyDynamicAudioState(
        float volumeMultiplier,
        float lpfFrequency,
        bool lpfRequested,
        float idleEffectAmount,
        float speedbreakerEffectAmount
    )
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        if (volumeMultiplier < 0.0f)
            volumeMultiplier = 0.0f;

        if (volumeMultiplier > 1.0f)
            volumeMultiplier = 1.0f;

        if (lpfFrequency < 300.0f)
            lpfFrequency = 300.0f;

        if (lpfFrequency > 20000.0f)
            lpfFrequency = 20000.0f;

        if (idleEffectAmount < 0.0f)
            idleEffectAmount = 0.0f;

        if (idleEffectAmount > 1.0f)
            idleEffectAmount = 1.0f;

        if (speedbreakerEffectAmount < 0.0f)
            speedbreakerEffectAmount = 0.0f;

        if (speedbreakerEffectAmount > 1.0f)
            speedbreakerEffectAmount = 1.0f;

        g_dynamicVolumeMultiplier =
            volumeMultiplier;

        g_dynamicLowPassFrequency =
            lpfFrequency;

        g_dynamicLowPassRequested =
            lpfRequested;

        g_idleEffectAmount =
            idleEffectAmount;

        g_idleMonoAmount =
            IDLE_MONO_AMOUNT_MAX *
            g_idleEffectAmount;

        g_speedbreakerEffectAmount =
            speedbreakerEffectAmount;

        if (g_currentStream)
        {
            BASS_ChannelSetAttribute(
                g_currentStream,
                BASS_ATTRIB_VOL,
                GetFinalVolume()
            );

            if (g_lowPassFX)
            {
                ApplyEffectiveLowPassFrequency();

                const bool shouldEnable =
                    ShouldLowPassBeActive();

                BASS_FXSetBypass(
                    g_lowPassFX,
                    shouldEnable ? FALSE : TRUE
                );

                g_lowPassEnabled = shouldEnable;
            }

            ApplyIdleEQ();
            ApplyIdleReverb();
        }
    }

    bool Init()
    {
        if (!BASS_Init(-1, 44100, 0, nullptr, nullptr))
        {
            Logger::Error("BASS_Init failed.");
            return false;
        }

        Logger::Info("BASS initialized successfully.");

        g_dynamicVolumeMultiplier = 1.0f;
        g_idleEffectAmount = 0.0f;
        g_idleMonoAmount = 0.0f;
        g_speedbreakerEffectAmount = 0.0f;

        SetVolume(Settings::GetMasterVolume());

        // Force-load BASS_FX.
        const DWORD fxVersion = BASS_FX_GetVersion();

        if (!fxVersion)
        {
            Logger::Error("BASS_FX failed to load.");
            BASS_Free();
            return false;
        }

        Logger::Info("BASS_FX initialized successfully.");

        SetLowPassFrequency(
            Settings::GetPausedLPFFrequency()
        );

        SetLowPassQ(
            Settings::GetPausedLPFQ()
        );

        SetFEEffectMix(
            Settings::GetFEEffectMix()
        );

        SetFEReverbEnabled(
            false
        );

        SetFEEQEnabled(
            false
        );

        return true;
    }

    void Shutdown()
    {
        Stop();

        g_lowPassRequested = false;
        g_lowPassEnabled = false;

        g_dynamicLowPassFrequency = 20000.0f;
        g_dynamicLowPassRequested = false;

        g_dynamicVolumeMultiplier = 1.0f;
        g_idleEffectAmount = 0.0f;
        g_idleMonoAmount = 0.0f;

        g_speedbreakerEffectAmount = 0.0f;

        BASS_Free();

        Logger::Info("BASS shutdown.");
    }

    bool Play(const std::string& filename)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        Logger::Info(
            ("AudioEngine::Play requested: " + filename).c_str()
        );

        Stop();

        g_currentStream = BASS_StreamCreateFile(
            FALSE,
            filename.c_str(),
            0,
            0,
            BASS_SAMPLE_FLOAT
        );

        g_idleStereoWidthDSP =
            BASS_ChannelSetDSP(
                g_currentStream,
                IdleStereoWidthDSP,
                nullptr,
                0
            );

        if (!g_idleStereoWidthDSP)
        {
            Logger::Error(
                "Failed to create IAOI stereo width DSP."
            );

            BASS_StreamFree(g_currentStream);
            g_currentStream = 0;

            return false;
        }

        if (!g_currentStream)
        {
            Logger::Error("Failed to create BASS stream.");
            return false;
        }

        // ---------------------------------------------------------
        // LOW-PASS FILTER
        // ---------------------------------------------------------
        g_lowPassFX = BASS_ChannelSetFX(
            g_currentStream,
            BASS_FX_BFX_BQF,
            0
        );

        if (!g_lowPassFX)
        {
            Logger::Error("Failed to create BASS low-pass FX.");

            BASS_StreamFree(g_currentStream);
            g_currentStream = 0;

            return false;
        }

        BASS_BFX_BQF lowPass = {};

        lowPass.lFilter = BASS_BFX_BQF_LOWPASS;

        lowPass.fCenter =
            GetEffectiveLowPassFrequency();

        lowPass.fBandwidth = 0.0f;
        lowPass.fQ =
            GetEffectiveLowPassQ();

        lowPass.fGain = 0.0f;
        lowPass.fS = 0.0f;

        lowPass.lChannel = BASS_BFX_CHANALL;

        if (!BASS_FXSetParameters(g_lowPassFX, &lowPass))
        {
            Logger::Error("Failed to set BASS low-pass parameters.");

            BASS_ChannelRemoveFX(g_currentStream, g_lowPassFX);
            g_lowPassFX = 0;

            BASS_StreamFree(g_currentStream);
            g_currentStream = 0;

            return false;
        }

        // Apply the currently requested LPF state to the new stream.
        //
        // This is important when a track changes while the game is
        // paused. The new track must inherit the existing LPF state.
        const bool shouldEnable =
            ShouldLowPassBeActive();

        BASS_FXSetBypass(
            g_lowPassFX,
            shouldEnable ? FALSE : TRUE
        );

        g_lowPassEnabled = shouldEnable;

        // ---------------------------------------------------------
        // IAOI LOW-END BOOST
        // ---------------------------------------------------------

        g_idleEQFX = BASS_ChannelSetFX(
            g_currentStream,
            BASS_FX_BFX_PEAKEQ,
            -80
        );

        if (!g_idleEQFX)
        {
            Logger::Error(
                "Failed to create IAOI low-end EQ."
            );

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_lowPassFX
            );

            g_lowPassFX = 0;

            BASS_StreamFree(
                g_currentStream
            );

            g_currentStream = 0;

            return false;
        }

        BASS_BFX_PEAKEQ idleEQ = {};

        idleEQ.lBand = 0;
        idleEQ.fCenter = IDLE_LOW_BOOST_CENTER;
        idleEQ.fGain =
            IDLE_LOW_BOOST_MAX_GAIN *
            g_idleEffectAmount;
        idleEQ.fBandwidth =
            IDLE_LOW_BOOST_BANDWIDTH;
        idleEQ.fQ = 0.0f;
        idleEQ.lChannel =
            BASS_BFX_CHANALL;

        if (!BASS_FXSetParameters(
            g_idleEQFX,
            &idleEQ
        ))
        {
            Logger::Error(
                "Failed to set IAOI low-end EQ."
            );

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_idleEQFX
            );

            g_idleEQFX = 0;

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_lowPassFX
            );

            g_lowPassFX = 0;

            BASS_StreamFree(
                g_currentStream
            );

            g_currentStream = 0;

            return false;
        }

        // ---------------------------------------------------------
        // IAOI REVERB
        // ---------------------------------------------------------

        g_idleReverbFX = BASS_ChannelSetFX(
            g_currentStream,
            BASS_FX_BFX_FREEVERB,
            -70
        );

        if (!g_idleReverbFX)
        {
            Logger::Error(
                "Failed to create IAOI reverb FX."
            );

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_idleEQFX
            );

            g_idleEQFX = 0;

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_lowPassFX
            );

            g_lowPassFX = 0;

            BASS_StreamFree(
                g_currentStream
            );

            g_currentStream = 0;

            return false;
        }

        BASS_BFX_FREEVERB idleReverb = {};

        idleReverb.fDryMix = 0.85f;
        idleReverb.fWetMix = 0.15f * g_idleEffectAmount;
        idleReverb.fRoomSize = 0.15f;
        idleReverb.fDamp = 0.60f;
        idleReverb.fWidth = 0.70f;
        idleReverb.lMode = 0;
        idleReverb.lChannel = BASS_BFX_CHANALL;

        if (!BASS_FXSetParameters(
            g_idleReverbFX,
            &idleReverb
        ))
        {
            Logger::Error(
                "Failed to set IAOI reverb parameters."
            );

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_idleReverbFX
            );

            g_idleReverbFX = 0;

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_idleEQFX
            );

            g_idleEQFX = 0;

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_lowPassFX
            );

            g_lowPassFX = 0;

            BASS_StreamFree(
                g_currentStream
            );

            g_currentStream = 0;

            return false;
        }

        // ---------------------------------------------------------
        // FRONTEND REVERB
        // ---------------------------------------------------------

        g_feReverbFX = BASS_ChannelSetFX(
            g_currentStream,
            BASS_FX_BFX_FREEVERB,
            -100
        );

        if (!g_feReverbFX)
        {
            Logger::Error("Failed to create BASS FE reverb FX.");

            BASS_ChannelRemoveFX(g_currentStream, g_lowPassFX);
            g_lowPassFX = 0;

            BASS_StreamFree(g_currentStream);
            g_currentStream = 0;

            return false;
        }

        BASS_BFX_FREEVERB reverb = {};

        reverb.fDryMix = 0.5f;
        reverb.fWetMix = g_feEffectMix / 100.0f;
        reverb.fRoomSize = 0.50f;
        reverb.fDamp = 0.45f;
        reverb.fWidth = 1.0f;
        reverb.lMode = 0;
        reverb.lChannel = BASS_BFX_CHANALL;

        if (!BASS_FXSetParameters(
            g_feReverbFX,
            &reverb
        ))
        {
            Logger::Error(
                "Failed to set BASS FE reverb parameters."
            );

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_feReverbFX
            );

            g_feReverbFX = 0;

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_lowPassFX
            );

            g_lowPassFX = 0;

            BASS_StreamFree(g_currentStream);
            g_currentStream = 0;

            return false;
        }

        BASS_FXSetBypass(
            g_feReverbFX,
            g_feReverbRequested ? FALSE : TRUE
        );

        g_feReverbEnabled = g_feReverbRequested;

        // ---------------------------------------------------------
// FRONTEND EQ
// ---------------------------------------------------------

        g_feEQFX = BASS_ChannelSetFX(
            g_currentStream,
            BASS_FX_BFX_PEAKEQ,
            -90
        );

        if (!g_feEQFX)
        {
            Logger::Error("Failed to create BASS FE EQ FX.");

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_feReverbFX
            );

            g_feReverbFX = 0;

            BASS_ChannelRemoveFX(
                g_currentStream,
                g_lowPassFX
            );

            g_lowPassFX = 0;

            BASS_StreamFree(g_currentStream);
            g_currentStream = 0;

            return false;
        }

        const float mix = g_feEffectMix / 100.0f;

        BASS_BFX_PEAKEQ eq = {};

        eq.fBandwidth = 0.8f;
        eq.fQ = 0.0f;
        eq.lChannel = BASS_BFX_CHANALL;

        // ---------------------------------------------------------
        // BAND 0 - LOW
        // ---------------------------------------------------------

        eq.lBand = 0;
        eq.fCenter = 120.0f;
        eq.fGain = -1.5f * mix;

        if (!BASS_FXSetParameters(
            g_feEQFX,
            &eq
        ))
        {
            Logger::Error("Failed to set BASS FE EQ low band.");
            return false;
        }

        // ---------------------------------------------------------
        // BAND 1 - MID
        // ---------------------------------------------------------

        eq.lBand = 1;
        eq.fCenter = 1800.0f;
        eq.fGain = 1.0f * mix;

        if (!BASS_FXSetParameters(
            g_feEQFX,
            &eq
        ))
        {
            Logger::Error("Failed to set BASS FE EQ mid band.");
            return false;
        }

        // ---------------------------------------------------------
        // BAND 2 - HIGH
        // ---------------------------------------------------------

        eq.lBand = 2;
        eq.fCenter = 7500.0f;
        eq.fGain = -15.0f * mix;

        if (!BASS_FXSetParameters(
            g_feEQFX,
            &eq
        ))
        {
            Logger::Error("Failed to set BASS FE EQ high band.");
            return false;
        }

        BASS_FXSetBypass(
            g_feEQFX,
            g_feEQRequested ? FALSE : TRUE
        );

        g_feEQEnabled = g_feEQRequested;

        // ---------------------------------------------------------
        // TRACK END
        // ---------------------------------------------------------
        BASS_ChannelSetSync(
            g_currentStream,
            BASS_SYNC_END,
            0,
            TrackEndSync,
            nullptr
        );

        BASS_ChannelSetAttribute(
            g_currentStream,
            BASS_ATTRIB_VOL,
            GetFinalVolume()
        );

        if (!BASS_ChannelPlay(g_currentStream, FALSE))
        {
            Logger::Error("Failed to play BASS stream.");

            BASS_ChannelRemoveFX(g_currentStream, g_lowPassFX);
            g_lowPassFX = 0;

            BASS_StreamFree(g_currentStream);
            g_currentStream = 0;

            return false;
        }

        Logger::Info("Audio playback started.");

        return true;
    }

    void Stop()
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (!g_currentStream)
            return;

        Logger::Info("AudioEngine::Stop called.");

        HSTREAM streamToStop = g_currentStream;

        g_currentStream = 0;

        BASS_ChannelStop(streamToStop);

        if (g_lowPassFX)
        {
            BASS_ChannelRemoveFX(
                streamToStop,
                g_lowPassFX
            );

            g_lowPassFX = 0;
        }

        if (g_feReverbFX)
        {
            BASS_ChannelRemoveFX(
                streamToStop,
                g_feReverbFX
            );

            g_feReverbFX = 0;
        }

        if (g_feEQFX)
        {
            BASS_ChannelRemoveFX(
                streamToStop,
                g_feEQFX
            );

            g_feEQFX = 0;
        }

        if (g_idleEQFX)
        {
            BASS_ChannelRemoveFX(
                streamToStop,
                g_idleEQFX
            );

            g_idleEQFX = 0;
        }

        if (g_idleReverbFX)
        {
            BASS_ChannelRemoveFX(
                streamToStop,
                g_idleReverbFX
            );

            g_idleReverbFX = 0;
        }

        g_feReverbEnabled = false;
        g_feEQEnabled = false;
        
        if (g_idleStereoWidthDSP)
        {
            BASS_ChannelRemoveDSP(
                streamToStop,
                g_idleStereoWidthDSP
            );

            g_idleStereoWidthDSP = 0;
        }
        g_idleMonoAmount = 0.0f;

        BASS_StreamFree(streamToStop);

        g_lowPassEnabled = false;
    }

    void Pause()
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (!g_currentStream)
            return;

        if (!IsPlaying())
            return;

        if (!BASS_ChannelPause(g_currentStream))
        {
            Logger::Error(
                "AudioEngine::Pause failed."
            );
            return;
        }

        Logger::Info(
            "AudioEngine: paused."
        );
    }

    void Resume()
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (!g_currentStream)
            return;

        if (!IsPaused())
            return;

        if (!BASS_ChannelPlay(g_currentStream, FALSE))
        {
            Logger::Error(
                "AudioEngine::Resume failed."
            );
            return;
        }

        Logger::Info(
            "AudioEngine: resumed."
        );
    }

    bool IsPlaying()
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (!g_currentStream)
            return false;

        const DWORD status =
            BASS_ChannelIsActive(g_currentStream);

        return status == BASS_ACTIVE_PLAYING;
    }

    bool IsPaused()
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (!g_currentStream)
            return false;

        const DWORD status = BASS_ChannelIsActive(g_currentStream);

        return status == BASS_ACTIVE_PAUSED;
    }

    void SetVolume(float volume)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (volume < 0.0f)
            volume = 0.0f;

        if (volume > 1.0f)
            volume = 1.0f;

        if (g_muted)
        {
            g_volumeBeforeMute = volume;
            return;
        }

        g_volume = volume;

        if (g_currentStream)
        {
            BASS_ChannelSetAttribute(
                g_currentStream,
                BASS_ATTRIB_VOL,
                GetFinalVolume()
            );
        }
    }

    void ToggleMute()
    {
        if (g_muted)
        {
            g_volume = g_volumeBeforeMute;
            g_muted = false;
        }
        else
        {
            g_volumeBeforeMute = g_volume;
            g_volume = 0.0f;
            g_muted = true;
        }

        if (g_currentStream)
        {
            BASS_ChannelSetAttribute(
                g_currentStream,
                BASS_ATTRIB_VOL,
                GetFinalVolume()
            );
        }

        Logger::Info(
            g_muted
            ? "AudioEngine: MUTED."
            : "AudioEngine: UNMUTED."
        );
    }

    bool IsMuted()
    {
        return g_muted;
    }

    void SetLowPassEnabled(bool enabled)
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        g_lowPassRequested = enabled;

        if (!g_lowPassFX)
            return;

        ApplyEffectiveLowPassFrequency();

        const bool shouldEnable =
            ShouldLowPassBeActive();

        BASS_FXSetBypass(
            g_lowPassFX,
            shouldEnable ? FALSE : TRUE
        );

        g_lowPassEnabled = shouldEnable;

        if (shouldEnable)
            Logger::Info("Audio LPF: ON");
        else
            Logger::Info("Audio LPF: OFF");
    }

    bool IsLowPassEnabled()
    {
        return g_lowPassEnabled;
    }

    void SetLowPassFrequency(float frequency)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (frequency < 100.0f)
            frequency = 100.0f;

        if (frequency > 20000.0f)
            frequency = 20000.0f;

        g_lowPassFrequency = frequency;

        if (!g_lowPassFX)
            return;

        if (!g_lowPassRequested)
            return;

        BASS_BFX_BQF lowPass = {};

        if (!BASS_FXGetParameters(
            g_lowPassFX,
            &lowPass
        ))
        {
            Logger::Error(
                "Failed to get BASS low-pass parameters."
            );
            return;
        }

        lowPass.fCenter = g_lowPassFrequency;

        if (!BASS_FXSetParameters(
            g_lowPassFX,
            &lowPass
        ))
        {
            Logger::Error(
                "Failed to update BASS low-pass frequency."
            );
        }
    }

    float GetLowPassFrequency()
    {
        return g_lowPassFrequency;
    }

    void SetLowPassQ(float q)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (q < 0.1f)
            q = 0.1f;

        if (q > 10.0f)
            q = 10.0f;

        g_lowPassQ = q;

        if (!g_lowPassFX)
            return;

        if (!g_lowPassRequested)
            return;

        BASS_BFX_BQF lowPass = {};

        if (!BASS_FXGetParameters(
            g_lowPassFX,
            &lowPass
        ))
        {
            Logger::Error("Failed to get BASS low-pass parameters.");
            return;
        }

        lowPass.fQ = g_lowPassQ;

        if (!BASS_FXSetParameters(
            g_lowPassFX,
            &lowPass
        ))
        {
            Logger::Error("Failed to update BASS low-pass Q.");
        }
    }

    void SetFEReverbEnabled(bool enabled)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        g_feReverbRequested = enabled;

        if (!g_feReverbFX)
            return;

        if (g_feReverbEnabled == enabled)
            return;

        BASS_FXSetBypass(
            g_feReverbFX,
            enabled ? FALSE : TRUE
        );

        g_feReverbEnabled = enabled;

        Logger::Info(
            enabled
            ? "Audio FE Reverb: ON"
            : "Audio FE Reverb: OFF"
        );
    }

    bool IsFEReverbEnabled()
    {
        return g_feReverbEnabled;
    }

    void SetFEEQEnabled(bool enabled)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        g_feEQRequested = enabled;

        if (!g_feEQFX)
            return;

        if (g_feEQEnabled == enabled)
            return;

        BASS_FXSetBypass(
            g_feEQFX,
            enabled ? FALSE : TRUE
        );

        g_feEQEnabled = enabled;

        Logger::Info(
            enabled
            ? "Audio FE EQ: ON"
            : "Audio FE EQ: OFF"
        );
    }

    bool IsFEEQEnabled()
    {
        return g_feEQEnabled;
    }

    void SetFEEffectMix(float mix)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);

        if (mix < 0.0f)
            mix = 0.0f;

        if (mix > 100.0f)
            mix = 100.0f;

        g_feEffectMix = mix;

        // ---------------------------------------------------------
        // REVERB MIX
        // ---------------------------------------------------------

        if (g_feReverbFX)
        {
            BASS_BFX_FREEVERB reverb = {};

            if (BASS_FXGetParameters(
                g_feReverbFX,
                &reverb
            ))
            {
                reverb.fWetMix =
                    g_feEffectMix / 100.0f;

                if (!BASS_FXSetParameters(
                    g_feReverbFX,
                    &reverb
                ))
                {
                    Logger::Error(
                        "Failed to update BASS FE reverb mix."
                    );
                }
            }
        }

        // ---------------------------------------------------------
        // EQ MIX
        // ---------------------------------------------------------

        if (g_feEQFX)
        {
            const float normalizedMix =
                g_feEffectMix / 100.0f;

            BASS_BFX_PEAKEQ eq = {};

            // LOW
            eq.lBand = 0;

            if (BASS_FXGetParameters(
                g_feEQFX,
                &eq
            ))
            {
                eq.fGain =
                    -1.5f * normalizedMix;

                BASS_FXSetParameters(
                    g_feEQFX,
                    &eq
                );
            }

            // MID
            eq.lBand = 1;

            if (BASS_FXGetParameters(
                g_feEQFX,
                &eq
            ))
            {
                eq.fGain =
                    1.0f * normalizedMix;

                BASS_FXSetParameters(
                    g_feEQFX,
                    &eq
                );
            }

            // HIGH
            eq.lBand = 2;

            if (BASS_FXGetParameters(
                g_feEQFX,
                &eq
            ))
            {
                eq.fGain =
                    -15.0f * normalizedMix;

                BASS_FXSetParameters(
                    g_feEQFX,
                    &eq
                );
            }
        }
    }

    float GetFEEffectMix()
    {
        return g_feEffectMix;
    }

    float GetLowPassQ()
    {
        return g_lowPassQ;
    }

    void SetDynamicVolumeMultiplier(float multiplier)
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        if (multiplier < 0.0f)
            multiplier = 0.0f;

        if (multiplier > 1.0f)
            multiplier = 1.0f;

        g_dynamicVolumeMultiplier = multiplier;

        if (g_currentStream)
        {
            BASS_ChannelSetAttribute(
                g_currentStream,
                BASS_ATTRIB_VOL,
                GetFinalVolume()
            );
        }
    }

    float GetDynamicVolumeMultiplier()
    {
        return g_dynamicVolumeMultiplier;
    }

    void SetDynamicLowPassFrequency(float frequency)
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        if (frequency < 300.0f)
            frequency = 300.0f;

        if (frequency > 20000.0f)
            frequency = 20000.0f;

        g_dynamicLowPassFrequency = frequency;

        if (g_lowPassRequested)
            return;

        ApplyEffectiveLowPassFrequency();
    }

    void SetDynamicLowPassEnabled(bool enabled)
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        g_dynamicLowPassRequested = enabled;

        if (!g_lowPassFX)
            return;

        const bool shouldEnable =
            g_lowPassRequested ||
            g_dynamicLowPassRequested;

        BASS_FXSetBypass(
            g_lowPassFX,
            shouldEnable ? FALSE : TRUE
        );

        g_lowPassEnabled = shouldEnable;
    }

    float GetDynamicLowPassFrequency()
    {
        return g_dynamicLowPassFrequency;
    }

    void SetIdleEffectAmount(float amount)
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        if (amount < 0.0f)
            amount = 0.0f;

        if (amount > 1.0f)
            amount = 1.0f;

        g_idleEffectAmount = amount;

        g_idleMonoAmount =
            IDLE_MONO_AMOUNT_MAX *
            g_idleEffectAmount;

        if (g_lowPassFX)
        {
            ApplyEffectiveLowPassFrequency();
        }

        ApplyIdleEQ();
        ApplyIdleReverb();
    }

    void SetSpeedbreakerEffectAmount(float amount)
    {
        std::lock_guard<std::recursive_mutex> lock(
            g_audioMutex
        );

        if (amount < 0.0f)
            amount = 0.0f;

        if (amount > 1.0f)
            amount = 1.0f;

        g_speedbreakerEffectAmount = amount;

        if (g_lowPassFX)
        {
            ApplyEffectiveLowPassFrequency();
        }
    }

    bool IsDynamicLowPassEnabled()
    {
        return g_dynamicLowPassRequested;
    }

    void SetTrackEndCallback(TrackEndCallback callback)
    {
        std::lock_guard<std::recursive_mutex> lock(g_audioMutex);
        g_trackEndCallback = callback;
    }
}