#include "pch.h"

#include "settings.h"
#include "paths.h"

#include "../src/logger.h"
#include "../audio/audio_engine.h"

#include <windows.h>
#include <string>

namespace
{
    // ---------------------------------------------------------
    // DEFAULTS
    // ---------------------------------------------------------

    float g_masterVolume = 1.0f;

    bool g_pausedLPFEnabled = true;
    float g_pausedLPFFrequency = 700.0f;
    float g_pausedLPFQ = 0.707f;
    float g_pausedLPFMix = 100.0f;

    bool g_feReverbEnabled = false;
    bool g_feEQEnabled = false;
    float g_feEffectMix = 25.0f;

    bool g_speedSensitiveAudioEnabled = false;
    bool g_interiorAudioOnIdleEnabled = false;
    bool g_speedbreakerAudioEnabled = false;

    int g_uiToggleKey = VK_F8;
    int g_menuToggleKey = '7';

    int g_menuUpKey = 'I';
    int g_menuDownKey = 'K';
    int g_menuLeftKey = 'J';
    int g_menuRightKey = 'L';
    int g_menuBackKey = 'U';
    int g_menuEnterKey = 'O';

    int g_playerPreviousKey = '8';
    int g_playerPlayPauseKey = '9';
    int g_playerNextKey = '0';
    int g_playerShuffleKey = '6';
    int g_playerMuteKey = '5';

    int g_miniPlayerSkin = 1;

    std::string g_iniPath;

    int ReadInt(
        const char* section,
        const char* key,
        int defaultValue
    )
    {
        return static_cast<int>(
            GetPrivateProfileIntA(
                section,
                key,
                defaultValue,
                g_iniPath.c_str()
            )
            );
    }

    float ReadFloat(
        const char* section,
        const char* key,
        float defaultValue
    )
    {
        char buffer[64] = {};

        sprintf_s(
            buffer,
            "%.6f",
            defaultValue
        );

        char value[64] = {};

        GetPrivateProfileStringA(
            section,
            key,
            buffer,
            value,
            sizeof(value),
            g_iniPath.c_str()
        );

        return static_cast<float>(
            atof(value)
            );
    }

    bool ReadBool(
        const char* section,
        const char* key,
        bool defaultValue
    )
    {
        return ReadInt(
            section,
            key,
            defaultValue ? 1 : 0
        ) != 0;
    }

    void WriteInt(
        const char* section,
        const char* key,
        int value
    )
    {
        char buffer[64] = {};

        sprintf_s(
            buffer,
            "%d",
            value
        );

        WritePrivateProfileStringA(
            section,
            key,
            buffer,
            g_iniPath.c_str()
        );
    }

    void WriteFloat(
        const char* section,
        const char* key,
        float value
    )
    {
        char buffer[64] = {};

        sprintf_s(
            buffer,
            "%.6f",
            value
        );

        WritePrivateProfileStringA(
            section,
            key,
            buffer,
            g_iniPath.c_str()
        );
    }

    void WriteBool(
        const char* section,
        const char* key,
        bool value
    )
    {
        WriteInt(
            section,
            key,
            value ? 1 : 0
        );
    }
}

namespace Settings
{
    bool Init()
    {
        g_iniPath =
            Paths::GetModRoot() +
            "\\..\\CarbonDMP.ini";

        if (g_iniPath.empty())
        {
            Logger::Error(
                "Settings: mod root is empty."
            );

            return false;
        }

        // -----------------------------------------------------
        // MASTER
        // -----------------------------------------------------

        g_masterVolume =
            ReadFloat(
                "Audio",
                "MasterVolume",
                1.0f
            );

        // -----------------------------------------------------
        // PAUSED LPF
        // -----------------------------------------------------

        g_pausedLPFEnabled =
            ReadBool(
                "PausedLPF",
                "Enabled",
                true
            );

        g_pausedLPFFrequency =
            ReadFloat(
                "PausedLPF",
                "Frequency",
                700.0f
            );

        g_pausedLPFQ =
            ReadFloat(
                "PausedLPF",
                "Q",
                0.707f
            );

        g_pausedLPFMix =
            ReadFloat(
                "PausedLPF",
                "Mix",
                100.0f
            );

        // -----------------------------------------------------
        // FRONTEND
        // -----------------------------------------------------

        g_feReverbEnabled =
            ReadBool(
                "FrontendFX",
                "ReverbEnabled",
                false
            );

        g_feEQEnabled =
            ReadBool(
                "FrontendFX",
                "EQEnabled",
                false
            );

        g_feEffectMix =
            ReadFloat(
                "FrontendFX",
                "Mix",
                25.0f
            );

        // -----------------------------------------------------
        // DYNAMIC AUDIO
        // -----------------------------------------------------

        g_speedSensitiveAudioEnabled =
            ReadBool(
                "DynamicAudio",
                "SpeedSensitiveAudio",
                false
            );

        g_interiorAudioOnIdleEnabled =
            ReadBool(
                "DynamicAudio",
                "InteriorAudioOnIdle",
                false
            );

        g_speedbreakerAudioEnabled =
            ReadBool(
                "DynamicAudio",
                "SpeedbreakerAudio",
                false
            );

        // -----------------------------------------------------
        // MINIPLAYER
        // -----------------------------------------------------

        g_miniPlayerSkin =
            ReadInt(
                "UI",
                "MiniPlayerSkin",
                1
            );

        if (g_miniPlayerSkin < 0 ||
            g_miniPlayerSkin > 1)
        {
            g_miniPlayerSkin = 1;
        }

        // -----------------------------------------------------
        // KEYS
        // -----------------------------------------------------

        g_uiToggleKey =
            ReadInt(
                "Keys",
                "UIToggle",
                VK_F8
            );

        g_menuToggleKey =
            ReadInt(
                "Keys",
                "ToggleMenu",
                '7'
            );

        g_menuUpKey =
            ReadInt(
                "Keys",
                "MenuUp",
                'I'
            );

        g_menuDownKey =
            ReadInt(
                "Keys",
                "MenuDown",
                'K'
            );

        g_menuLeftKey =
            ReadInt(
                "Keys",
                "MenuLeft",
                'J'
            );

        g_menuRightKey =
            ReadInt(
                "Keys",
                "MenuRight",
                'L'
            );

        g_menuBackKey =
            ReadInt(
                "Keys",
                "MenuBack",
                'U'
            );

        g_menuEnterKey =
            ReadInt(
                "Keys",
                "MenuEnter",
                'O'
            );

        g_playerPreviousKey =
            ReadInt(
                "Keys",
                "PlayerPrevious",
                '8'
            );

        g_playerPlayPauseKey =
            ReadInt(
                "Keys",
                "PlayerPlayPause",
                '9'
            );

        g_playerNextKey =
            ReadInt(
                "Keys",
                "PlayerNext",
                '0'
            );

        g_playerShuffleKey =
            ReadInt(
                "Keys",
                "PlayerShuffle",
                '6'
            );

        g_playerMuteKey =
            ReadInt(
                "Keys",
                "PlayerMute",
                '5'
            );

        Logger::Info(
            "Settings initialized."
        );

        return true;
    }

    int GetMiniPlayerSkin()
    {
        return g_miniPlayerSkin;
    }

    void SetMiniPlayerSkin(int skin)
    {
        if (skin < 0)
            skin = 0;

        if (skin > 1)
            skin = 1;

        g_miniPlayerSkin = skin;
    }

    void Save()
    {
        if (g_iniPath.empty())
            return;

        WriteFloat(
            "Audio",
            "MasterVolume",
            g_masterVolume
        );

        WriteBool(
            "PausedLPF",
            "Enabled",
            g_pausedLPFEnabled
        );

        WriteFloat(
            "PausedLPF",
            "Frequency",
            g_pausedLPFFrequency
        );

        WriteFloat(
            "PausedLPF",
            "Q",
            g_pausedLPFQ
        );

        WriteFloat(
            "PausedLPF",
            "Mix",
            g_pausedLPFMix
        );

        WriteBool(
            "FrontendFX",
            "ReverbEnabled",
            g_feReverbEnabled
        );

        WriteBool(
            "FrontendFX",
            "EQEnabled",
            g_feEQEnabled
        );

        WriteFloat(
            "FrontendFX",
            "Mix",
            g_feEffectMix
        );

        WriteBool(
            "DynamicAudio",
            "SpeedSensitiveAudio",
            g_speedSensitiveAudioEnabled
        );

        WriteBool(
            "DynamicAudio",
            "InteriorAudioOnIdle",
            g_interiorAudioOnIdleEnabled
        );

        WriteBool(
            "DynamicAudio",
            "SpeedbreakerAudio",
            g_speedbreakerAudioEnabled
        );

        WriteInt(
            "UI",
            "MiniPlayerSkin",
            g_miniPlayerSkin
        );

        WriteInt(
            "Keys",
            "UIToggle",
            g_uiToggleKey
        );

        WriteInt(
            "Keys",
            "ToggleMenu",
            g_menuToggleKey
        );

        WriteInt(
            "Keys",
            "MenuUp",
            g_menuUpKey
        );

        WriteInt(
            "Keys",
            "MenuDown",
            g_menuDownKey
        );

        WriteInt(
            "Keys",
            "MenuLeft",
            g_menuLeftKey
        );

        WriteInt(
            "Keys",
            "MenuRight",
            g_menuRightKey
        );

        WriteInt(
            "Keys",
            "MenuBack",
            g_menuBackKey
        );

        WriteInt(
            "Keys",
            "MenuEnter",
            g_menuEnterKey
        );

        WriteInt(
            "Keys",
            "PlayerPrevious",
            g_playerPreviousKey
        );

        WriteInt(
            "Keys",
            "PlayerPlayPause",
            g_playerPlayPauseKey
        );

        WriteInt(
            "Keys",
            "PlayerNext",
            g_playerNextKey
        );

        WriteInt(
            "Keys",
            "PlayerShuffle",
            g_playerShuffleKey
        );

        WriteInt(
            "Keys",
            "PlayerMute",
            g_playerMuteKey
        );
    }

    float GetMasterVolume()
    {
        return g_masterVolume;
    }

    void SetMasterVolume(float volume)
    {
        if (volume < 0.0f)
            volume = 0.0f;

        if (volume > 1.0f)
            volume = 1.0f;

        g_masterVolume = volume;
    }

    bool IsPausedLPFEnabled()
    {
        return g_pausedLPFEnabled;
    }

    void SetPausedLPFEnabled(bool enabled)
    {
        g_pausedLPFEnabled = enabled;
    }

    float GetPausedLPFFrequency()
    {
        return g_pausedLPFFrequency;
    }

    void SetPausedLPFFrequency(float frequency)
    {
        g_pausedLPFFrequency = frequency;
    }

    float GetPausedLPFQ()
    {
        return g_pausedLPFQ;
    }

    void SetPausedLPFQ(float q)
    {
        g_pausedLPFQ = q;
    }

    float GetPausedLPFMix()
    {
        return g_pausedLPFMix;
    }

    void SetPausedLPFMix(float mix)
    {
        g_pausedLPFMix = mix;
    }

    bool IsFEReverbEnabled()
    {
        return g_feReverbEnabled;
    }

    void SetFEReverbEnabled(bool enabled)
    {
        g_feReverbEnabled = enabled;
    }

    bool IsFEEQEnabled()
    {
        return g_feEQEnabled;
    }

    void SetFEEQEnabled(bool enabled)
    {
        g_feEQEnabled = enabled;
    }

    float GetFEEffectMix()
    {
        return g_feEffectMix;
    }

    void SetFEEffectMix(float mix)
    {
        g_feEffectMix = mix;
    }

    bool IsSpeedSensitiveAudioEnabled()
    {
        return g_speedSensitiveAudioEnabled;
    }

    void SetSpeedSensitiveAudioEnabled(bool enabled)
    {
        g_speedSensitiveAudioEnabled = enabled;
    }

    bool IsInteriorAudioOnIdleEnabled()
    {
        return g_interiorAudioOnIdleEnabled;
    }

    void SetInteriorAudioOnIdleEnabled(bool enabled)
    {
        g_interiorAudioOnIdleEnabled = enabled;
    }

    bool IsSpeedbreakerAudioEnabled()
    {
        return g_speedbreakerAudioEnabled;
    }

    void SetSpeedbreakerAudioEnabled(bool enabled)
    {
        g_speedbreakerAudioEnabled = enabled;
    }

    int GetUIToggleKey()
    {
        return g_uiToggleKey;
    }

    void SetUIToggleKey(int key)
    {
        g_uiToggleKey = key;
    }

    int GetToggleMenuKey()
    {
        return g_menuToggleKey;
    }

    void SetToggleMenuKey(int key)
    {
        g_menuToggleKey = key;
    }

    int GetMenuUpKey()
    {
        return g_menuUpKey;
    }

    void SetMenuUpKey(int key)
    {
        g_menuUpKey = key;
    }

    int GetMenuDownKey()
    {
        return g_menuDownKey;
    }

    void SetMenuDownKey(int key)
    {
        g_menuDownKey = key;
    }

    int GetMenuLeftKey()
    {
        return g_menuLeftKey;
    }

    void SetMenuLeftKey(int key)
    {
        g_menuLeftKey = key;
    }

    int GetMenuRightKey()
    {
        return g_menuRightKey;
    }

    void SetMenuRightKey(int key)
    {
        g_menuRightKey = key;
    }

    int GetMenuBackKey()
    {
        return g_menuBackKey;
    }

    void SetMenuBackKey(int key)
    {
        g_menuBackKey = key;
    }

    int GetMenuEnterKey()
    {
        return g_menuEnterKey;
    }

    void SetMenuEnterKey(int key)
    {
        g_menuEnterKey = key;
    }

    int GetPlayerPreviousKey()
    {
        return g_playerPreviousKey;
    }

    void SetPlayerPreviousKey(int key)
    {
        g_playerPreviousKey = key;
    }

    int GetPlayerPlayPauseKey()
    {
        return g_playerPlayPauseKey;
    }

    void SetPlayerPlayPauseKey(int key)
    {
        g_playerPlayPauseKey = key;
    }

    int GetPlayerNextKey()
    {
        return g_playerNextKey;
    }

    void SetPlayerNextKey(int key)
    {
        g_playerNextKey = key;
    }

    int GetPlayerShuffleKey()
    {
        return g_playerShuffleKey;
    }

    void SetPlayerShuffleKey(int key)
    {
        g_playerShuffleKey = key;
    }

    int GetPlayerMuteKey()
    {
        return g_playerMuteKey;
    }

    void SetPlayerMuteKey(int key)
    {
        g_playerMuteKey = key;
    }

    void ApplyAudioSettings()
    {
        AudioEngine::SetVolume(g_masterVolume);

        AudioEngine::SetLowPassFrequency(
            g_pausedLPFFrequency
        );

        AudioEngine::SetLowPassQ(
            g_pausedLPFQ
        );

        AudioEngine::SetFEEffectMix(
            g_feEffectMix
        );
    }
}