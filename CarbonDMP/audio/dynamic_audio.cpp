#include "pch.h"

#include "dynamic_audio.h"

#include "../audio/audio_engine.h"
#include "../config/settings.h"
#include "../game/game_state.h"
#include "../game/game_memory.h"
#include "../src/logger.h"

#include <cmath>
#include <algorithm>

namespace
{
    bool g_initialized = false;

    bool g_speedSensitiveEnabled = false;
    bool g_interiorAudioOnIdleEnabled = false;
    bool g_speedbreakerAudioEnabled = false;

    struct DynamicAudioTarget
    {
        float volumeMultiplier = 1.0f;
        float lpfCutoff = 20000.0f;
    };

    DynamicAudioTarget g_target;
    DynamicAudioTarget g_current;

    float g_idleTimer = 0.0f;
    float g_idleEffectAmount = 0.0f;

    constexpr float IDLE_TRIGGER_TIME = 12.5f;
    constexpr float IDLE_TRANSITION_IN_TIME = 1.0f;
    constexpr float IDLE_TRANSITION_OUT_TIME = 0.35f;
    constexpr float IDLE_SPEED_THRESHOLD = 0.5f;
    constexpr float IDLE_VOLUME_PERCENT = 0.60f;

    float g_speedbreakerEffectAmount = 0.0f;

    constexpr uintptr_t SPEEDBREAKER_ADDRESS = 0x00A9922C;

    constexpr float SPEEDBREAKER_TRANSITION_TIME = 0.35f;
}

namespace DynamicAudio
{
    bool Init()
    {
        g_speedSensitiveEnabled =
            Settings::IsSpeedSensitiveAudioEnabled();

        g_interiorAudioOnIdleEnabled =
            Settings::IsInteriorAudioOnIdleEnabled();

        g_speedbreakerAudioEnabled =
            Settings::IsSpeedbreakerAudioEnabled();

        g_initialized = true;

        Logger::Info(
            "DynamicAudio initialized."
        );

        return true;
    }

    void Shutdown()
    {
        if (!g_initialized)
            return;

        g_speedSensitiveEnabled = false;
        g_interiorAudioOnIdleEnabled = false;
        g_initialized = false;
        g_idleTimer = 0.0f;
        g_idleEffectAmount = 0.0f;
        g_speedbreakerAudioEnabled = false;
        g_speedbreakerEffectAmount = 0.0f;

        Logger::Info(
            "DynamicAudio shutdown."
        );
    }

    constexpr uintptr_t PLAYER_SPEED_ADDRESS = 0x00A8E178;
    constexpr float RAW_SPEED_TO_KMH = 3.594f;

    float GetPlayerSpeedKmh()
    {
        const float rawSpeed =
            *reinterpret_cast<float*>(
                PLAYER_SPEED_ADDRESS
                );

        if (!std::isfinite(rawSpeed))
            return 0.0f;

        if (rawSpeed < 0.0f)
            return 0.0f;

        return rawSpeed * RAW_SPEED_TO_KMH;
    }

    /*float CalculateSSANormalized(float speed)
    {
        if (speed <= 40.0f)
            return 0.0f;

        if (speed >= 200.0f)
            return 1.0f;

        if (speed <= 100.0f)
        {
            const float t =
                (speed - 40.0f) /
                (100.0f - 40.0f);

            return t * t * (3.0f - 2.0f * t);
        }

        if (speed <= 150.0f)
        {
            const float t =
                (speed - 100.0f) /
                (150.0f - 100.0f);

            return t * t * (3.0f - 2.0f * t);
        }

        const float t =
            (speed - 150.0f) /
            (200.0f - 150.0f);

        return t * t * (3.0f - 2.0f * t);
    }OLD FUNCTION FOR BACKUP, NGACO, BOLAK BALIK MULU VOLUMENYA </3*/

    float CalculateSSANormalized(float speed)
    {
        constexpr float MIN_SPEED = 40.0f;
        constexpr float MAX_SPEED = 200.0f;

        if (speed <= MIN_SPEED)
            return 0.0f;

        if (speed >= MAX_SPEED)
            return 1.0f;

        const float t =
            (speed - MIN_SPEED) /
            (MAX_SPEED - MIN_SPEED);

        return t * t * (3.0f - 2.0f * t);
    }

    float CalculateSSAVolume(
        float normalized)
    {
        return 0.10f +
            (0.90f * normalized);
    }

    float CalculateSSACutoff(
        float normalized)
    {
        constexpr float MIN_CUTOFF = 1000.0f;
        constexpr float MAX_CUTOFF = 20000.0f;

        return MIN_CUTOFF +
            ((MAX_CUTOFF - MIN_CUTOFF) *
                normalized);
    }

    bool IsDrivingAudioAllowed()
    {
        if (!g_initialized)
            return false;

        if (GameStateManager::GetState() !=
            GameState::GAMEPLAY)
        {
            return false;
        }

        if (GameMemory::IsGameplayPaused())
        {
            return false;
        }

        return true;
    }


    bool IsSpeedbreakerActive()
    {
        const auto state =
            *reinterpret_cast<std::uint8_t*>(
                SPEEDBREAKER_ADDRESS
                );

        return state != 0;
    }

    float SmoothTowards(
        float current,
        float target,
        float dt,
        float response)
    {
        if (dt <= 0.0f)
            return current;

        const float alpha =
            1.0f - std::exp(
                -response * dt
            );

        return current +
            ((target - current) * alpha);
    }

    float UpdateSpeedbreakerEffectAmount(
        float current,
        float target,
        float dt);

    float UpdateIdleEffectAmount(
        float current,
        float target,
        float dt)
    {
        if (dt <= 0.0f)
            return current;

        const float transitionTime =
            target > current
            ? IDLE_TRANSITION_IN_TIME
            : IDLE_TRANSITION_OUT_TIME;

        if (transitionTime <= 0.0f)
            return target;

        const float step =
            dt / transitionTime;

        if (target > current)
            current += step;
        else
            current -= step;

        if (current < 0.0f)
            current = 0.0f;

        if (current > 1.0f)
            current = 1.0f;

        return current;
    }

    float CalculateEffectiveVolumeTarget()
    {
        const float normalVolume =
            g_target.volumeMultiplier;

        const float idleVolume =
            IDLE_VOLUME_PERCENT;

        return
            normalVolume +
            ((idleVolume - normalVolume) *
                g_idleEffectAmount);
    }

    void ApplySmoothedTargets(float dt)
    {
        constexpr float VOLUME_RESPONSE = 8.0f;
        constexpr float LPF_RESPONSE = 3.5f;

        const float effectiveVolumeTarget =
            CalculateEffectiveVolumeTarget();

        g_current.volumeMultiplier =
            SmoothTowards(
                g_current.volumeMultiplier,
                effectiveVolumeTarget,
                dt,
                VOLUME_RESPONSE
            );

        g_current.lpfCutoff =
            SmoothTowards(
                g_current.lpfCutoff,
                g_target.lpfCutoff,
                dt,
                LPF_RESPONSE
            );

        AudioEngine::ApplyDynamicAudioState(
            g_current.volumeMultiplier,
            g_current.lpfCutoff,
            IsDrivingAudioAllowed() &&
            (
                g_speedSensitiveEnabled ||
                g_idleEffectAmount > 0.0f ||
                g_speedbreakerEffectAmount > 0.0f
                ),
            g_idleEffectAmount,
            g_speedbreakerEffectAmount
        );
    }

    void UpdateIdleState(float dt)
    {
        if (!g_interiorAudioOnIdleEnabled)
        {
            g_idleTimer = 0.0f;
            g_idleEffectAmount = 0.0f;
            return;
        }

        if (GameMemory::IsGameplayPaused())
        {
            g_idleTimer = 0.0f;
            g_idleEffectAmount = 0.0f;
            return;
        }

        if (GameStateManager::GetState() !=
            GameState::GAMEPLAY)
        {
            g_idleTimer = 0.0f;
            g_idleEffectAmount = 0.0f;
            return;
        }

        const float speed =
            GetPlayerSpeedKmh();

        if (speed <= IDLE_SPEED_THRESHOLD)
        {
            g_idleTimer += dt;
        }
        else
        {
            g_idleTimer = 0.0f;
        }

        const bool idleTriggered =
            g_idleTimer >= IDLE_TRIGGER_TIME;

        const float target =
            idleTriggered ? 1.0f : 0.0f;

        g_idleEffectAmount =
            UpdateIdleEffectAmount(
                g_idleEffectAmount,
                target,
                dt
            );
    }

    void UpdateSpeedbreakerState(float dt)
    {
        if (!g_speedbreakerAudioEnabled ||
            !IsDrivingAudioAllowed())
        {
            g_speedbreakerEffectAmount = 0.0f;
            return;
        }

        const float target =
            IsSpeedbreakerActive()
            ? 1.0f
            : 0.0f;

        g_speedbreakerEffectAmount =
            UpdateSpeedbreakerEffectAmount(
                g_speedbreakerEffectAmount,
                target,
                dt
            );
    }

    float UpdateSpeedbreakerEffectAmount(
        float current,
        float target,
        float dt)
    {
        if (dt <= 0.0f)
            return current;

        const float step =
            dt / SPEEDBREAKER_TRANSITION_TIME;

        if (target > current)
            current += step;
        else
            current -= step;

        if (current < 0.0f)
            current = 0.0f;

        if (current > 1.0f)
            current = 1.0f;

        return current;
    }

    void Update(float dt)
    {
        if (!g_initialized)
            return;

        UpdateIdleState(dt);
        UpdateSpeedbreakerState(dt);

        if (!IsDrivingAudioAllowed())
        {
            g_target.volumeMultiplier = 1.0f;
            g_target.lpfCutoff = 20000.0f;

            ApplySmoothedTargets(dt);
            return;
        }

        static float iaoiLogTimer = 0.0f;

        iaoiLogTimer += dt;

        if (iaoiLogTimer >= 1.0f)
        {
            char buffer[256];

            const float speed =
                GetPlayerSpeedKmh();

            sprintf_s(
                buffer,
                "DynamicAudio IAOI: enabled=%d speed=%.1f idleTimer=%.1f amount=%.3f",
                g_interiorAudioOnIdleEnabled ? 1 : 0,
                speed,
                g_idleTimer,
                g_idleEffectAmount
            );

            Logger::Info(buffer);

            iaoiLogTimer = 0.0f;
        }

        if (g_speedSensitiveEnabled)
        {
            const float speed =
                GetPlayerSpeedKmh();

            static float ssaLogTimer = 0.0f;

            ssaLogTimer += dt;

            if (ssaLogTimer >= 1.0f)
            {
                char buffer[256];

                sprintf_s(
                    buffer,
                    "DynamicAudio SSA: enabled=%d speed=%.1f kmh targetVol=%.3f currentVol=%.3f",
                    g_speedSensitiveEnabled ? 1 : 0,
                    speed,
                    g_target.volumeMultiplier,
                    g_current.volumeMultiplier
                );

                Logger::Info(buffer);

                ssaLogTimer = 0.0f;
            }

            const float normalized =
                CalculateSSANormalized(speed);

            g_target.volumeMultiplier =
                CalculateSSAVolume(normalized);

            g_target.lpfCutoff =
                CalculateSSACutoff(normalized);
        }
        else
        {
            g_target.volumeMultiplier = 1.0f;
            g_target.lpfCutoff = 20000.0f;
        }

        ApplySmoothedTargets(dt);
    }


    void SetSpeedSensitiveEnabled(bool enabled)
    {
        g_speedSensitiveEnabled = enabled;
    }

    void SetInteriorAudioOnIdleEnabled(bool enabled)
    {
        g_interiorAudioOnIdleEnabled = enabled;
    }

    void SetSpeedbreakerAudioEnabled(bool enabled)
    {
        g_speedbreakerAudioEnabled = enabled;

        if (!enabled)
        {
            g_speedbreakerEffectAmount = 0.0f;
        }
    }

    bool IsSpeedSensitiveEnabled()
    {
        return g_speedSensitiveEnabled;
    }

    bool IsInteriorAudioOnIdleEnabled()
    {
        return g_interiorAudioOnIdleEnabled;
    }

    bool IsSpeedbreakerAudioEnabled()
    {
        return g_speedbreakerAudioEnabled;
    }
}