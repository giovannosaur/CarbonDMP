#pragma once

namespace Settings
{
    bool Init();
    void Save();

    // ---------------------------------------------------------
    // MASTER VOLUME
    // ---------------------------------------------------------

    float GetMasterVolume();
    void SetMasterVolume(float volume);

    // ---------------------------------------------------------
    // PAUSED LPF
    // ---------------------------------------------------------

    bool IsPausedLPFEnabled();
    void SetPausedLPFEnabled(bool enabled);

    float GetPausedLPFFrequency();
    void SetPausedLPFFrequency(float frequency);

    float GetPausedLPFQ();
    void SetPausedLPFQ(float q);

    float GetPausedLPFMix();
    void SetPausedLPFMix(float mix);

    // ---------------------------------------------------------
    // FRONTEND ROOM EFFECT
    // ---------------------------------------------------------

    bool IsFEReverbEnabled();
    void SetFEReverbEnabled(bool enabled);

    bool IsFEEQEnabled();
    void SetFEEQEnabled(bool enabled);

    float GetFEEffectMix();
    void SetFEEffectMix(float mix);

    // ---------------------------------------------------------
    // DYNAMIC AUDIO
    // ---------------------------------------------------------

    bool IsSpeedSensitiveAudioEnabled();
    void SetSpeedSensitiveAudioEnabled(bool enabled);

    bool IsInteriorAudioOnIdleEnabled();
    void SetInteriorAudioOnIdleEnabled(bool enabled);

    bool IsSpeedbreakerAudioEnabled();
    void SetSpeedbreakerAudioEnabled(bool enabled);

    // ---------------------------------------------------------
    // MINIPLAYER
    // ---------------------------------------------------------

    int GetMiniPlayerSkin();
    void SetMiniPlayerSkin(int skin);

    // ---------------------------------------------------------
    // KEY CONFIG
    // ---------------------------------------------------------

    int GetUIToggleKey();
    void SetUIToggleKey(int key);

    int GetToggleMenuKey();
    void SetToggleMenuKey(int key);

    int GetMenuUpKey();
    void SetMenuUpKey(int key);

    int GetMenuDownKey();
    void SetMenuDownKey(int key);

    int GetMenuLeftKey();
    void SetMenuLeftKey(int key);

    int GetMenuRightKey();
    void SetMenuRightKey(int key);

    int GetMenuBackKey();
    void SetMenuBackKey(int key);

    int GetMenuEnterKey();
    void SetMenuEnterKey(int key);

    int GetPlayerPreviousKey();
    void SetPlayerPreviousKey(int key);

    int GetPlayerPlayPauseKey();
    void SetPlayerPlayPauseKey(int key);

    int GetPlayerNextKey();
    void SetPlayerNextKey(int key);

    int GetPlayerShuffleKey();
    void SetPlayerShuffleKey(int key);

    int GetPlayerMuteKey();
    void SetPlayerMuteKey(int key);

    void ApplyAudioSettings();
}