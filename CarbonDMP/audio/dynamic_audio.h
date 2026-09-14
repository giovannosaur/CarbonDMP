#pragma once

namespace DynamicAudio
{
    bool Init();
    void Shutdown();

    void Update(float dt);

    bool IsDrivingAudioAllowed();

    void SetSpeedSensitiveEnabled(bool enabled);
    void SetInteriorAudioOnIdleEnabled(bool enabled);
    void SetSpeedbreakerAudioEnabled(bool enabled);

    bool IsSpeedSensitiveEnabled();
    bool IsInteriorAudioOnIdleEnabled();
    bool IsSpeedbreakerAudioEnabled();
}