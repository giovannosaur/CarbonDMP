#include "pch.h"

// Windows headers may define max/min as macros.
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include "music_player_ui.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <algorithm>
#include <cmath>

#include "../audio/music_manager.h"
#include "../audio/playlist.h"
#include "../audio/audio_engine.h"

#include "../src/logger.h"

#include "../config/settings.h"
#include "../config/paths.h"

#include "../game/game_state.h"
#include "../audio/dynamic_audio.h"

void UpdateFancyMiniPlayerHitboxes(
    const RECT& player);

namespace
{
    bool g_initialized = false;
    bool g_enabled = true;
    bool g_menuOpen = false;

    IDirect3DStateBlock9* g_uiStateBlock = nullptr;

    enum class MiniPlayerSkin
    {
        SIMPLE,
        IPOD_CLASSIC_5
    };

    MiniPlayerSkin g_miniPlayerSkin =
        MiniPlayerSkin::IPOD_CLASSIC_5;

    enum class MenuTab
    {
        MUSIC,
        SETTINGS,
        CREDITS
    };

    enum class MenuFocus
    {
        TABS,
        CONTENT,
        PLAYER
    };

    MenuTab g_currentTab = MenuTab::MUSIC;
    MenuFocus g_menuFocus = MenuFocus::TABS;

    int g_menuSelection = 0;

    int g_musicScrollOffset = 0;
    int g_settingsScrollOffset = 0;

    bool g_musicScrollbarDragging = false;
    bool g_settingsScrollbarDragging = false;

    LONG g_musicScrollbarDragOffset = 0;
    LONG g_settingsScrollbarDragOffset = 0;

    RECT g_musicScrollbarTrack = {};
    RECT g_musicScrollbarThumb = {};

    RECT g_settingsScrollbarTrack = {};
    RECT g_settingsScrollbarThumb = {};

    bool g_musicScrollbarHovered = false;
    bool g_settingsScrollbarHovered = false;

    bool GetMousePosition(POINT& point);
    void ScrollMusicFromScrollbarY(LONG mouseY);
    void ScrollSettingsFromScrollbarY(LONG mouseY);
    void AdjustSettingsValue(bool increase);

    int GetMusicVisibleRows();
    int GetMusicMaxScrollOffset();
    int GetSettingsVisibleRows();
    int GetSettingsMaxScrollOffset();

    void SetMusicScrollOffset(int offset);
    void SetSettingsScrollOffset(int offset);

    LPD3DXFONT g_font = nullptr;

    LPD3DXFONT g_miniFont = nullptr;

    int g_miniFontHeight = 0;

    LPDIRECT3DTEXTURE9
        g_fancyMiniPlayerTexture = nullptr;

    RECT g_playerRect = {};
    RECT g_previousButton = {};
    RECT g_playPauseButton = {};
    RECT g_nextButton = {};
    RECT g_menuButton = {};

    RECT g_miniPlayerTargetRect = {};

    float g_miniScreenX = 0.0f;
    float g_miniTargetScreenX = 0.0f;
    float g_miniAnimationStartX = 0.0f;
    float g_miniAnimationEndX = 0.0f;

    float g_miniAnimationElapsed = 0.0f;

    bool g_miniAnimationActive = false;
    bool g_miniWantedVisible = true;
    bool g_miniVisibilityInitialized = false;

    LONG g_miniLastScreenWidth = 0;
    LONG g_miniLastScreenHeight = 0;

    constexpr float MINI_ANIMATION_DURATION = 0.18f;
    constexpr float MINI_LEFT_MARGIN = 30.0f;
    constexpr float MINI_EXIT_MARGIN = 30.0f;

    struct MarqueeState
    {
        std::string text;
        float offset = 0.0f;
        float holdTime = 0.65f;
    };

    MarqueeState g_artistMarquee;
    MarqueeState g_titleMarquee;
    MarqueeState g_albumMarquee;

    MarqueeState g_selectedArtistMarquee;
    MarqueeState g_selectedTitleMarquee;
    MarqueeState g_selectedAlbumMarquee;

    MarqueeState g_menuArtistMarquee;
    MarqueeState g_menuTitleMarquee;

    constexpr float MARQUEE_SPEED = 18.0f;
    constexpr float MARQUEE_GAP = 28.0f;
    constexpr float MARQUEE_HOLD = 0.65f;
    constexpr LONG MARQUEE_FADE_WIDTH = 12;

    RECT g_mainMenuRect = {};
    RECT g_closeButton = {};

    RECT g_menuPreviousButton = {};
    RECT g_menuPlayPauseButton = {};
    RECT g_menuNextButton = {};

    RECT g_menuShuffleButton = {};
    RECT g_menuMuteButton = {};

    LPDIRECT3DTEXTURE9 g_previousTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_previousHoverTexture = nullptr;

    LPDIRECT3DTEXTURE9 g_playTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_playHoverTexture = nullptr;

    LPDIRECT3DTEXTURE9 g_pauseTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_pauseHoverTexture = nullptr;

    LPDIRECT3DTEXTURE9 g_nextTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_nextHoverTexture = nullptr;

    LPDIRECT3DTEXTURE9 g_shuffleTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_shuffleHoverTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_shuffleOnTexture = nullptr;

    LPDIRECT3DTEXTURE9 g_muteTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_muteHoverTexture = nullptr;
    LPDIRECT3DTEXTURE9 g_muteOnTexture = nullptr;

    LPDIRECT3DTEXTURE9 g_cursorTexture = nullptr;

    bool g_menuPreviousHovered = false;
    bool g_menuPlayPauseHovered = false;
    bool g_menuNextHovered = false;
    bool g_menuShuffleHovered = false;
    bool g_menuMuteHovered = false;

    std::vector<RECT> g_musicRowRects;
    std::vector<RECT> g_musicContextRects;

    bool g_previousHovered = false;
    bool g_playPauseHovered = false;
    bool g_nextHovered = false;
    bool g_menuHovered = false;
    bool g_closeHovered = false;
    bool g_musicTabHovered = false;
    bool g_settingsTabHovered = false;
    bool g_creditsTabHovered = false;

    bool g_musicRowHovered = false;
    int g_musicHoveredRow = -1;

    bool g_musicContextHovered = false;

    /*
    constexpr int UI_TOGGLE_KEY = VK_F8;

    constexpr int MENU_UP_KEY = 'I';
    constexpr int MENU_DOWN_KEY = 'K';
    constexpr int MENU_LEFT_KEY = 'J';
    constexpr int MENU_RIGHT_KEY = 'L';
    constexpr int MENU_BACK_KEY = 'U';
    constexpr int MENU_ENTER_KEY = 'O';
    */

    int g_uiToggleKey = VK_F8;
    int g_menuToggleKey = '7';

    constexpr LONG PLAYER_WIDTH = 360;
    constexpr LONG PLAYER_HEIGHT = 150;
    constexpr LONG PLAYER_MARGIN = 30;

    constexpr LONG BUTTON_SIZE = 40;
    constexpr LONG BUTTON_GAP = 15;

    constexpr LONG IPOD_WIDTH = 256;
    constexpr LONG IPOD_HEIGHT = 417;

    constexpr LONG MUSIC_ROW_HEIGHT = 32;
    constexpr LONG MUSIC_LIST_TOP = 155;
    constexpr LONG MUSIC_LIST_BOTTOM = 395;
    constexpr LONG MUSIC_COLUMN_GAP = 8;

    constexpr float UI_BASE_WIDTH = 1280.0f;
    constexpr float UI_BASE_HEIGHT = 720.0f;

    constexpr float UI_SCALE_FACTOR = 0.75f;

    float g_uiScale = 1.0f;
    float g_uiOffsetX = 0.0f;
    float g_uiOffsetY = 0.0f;

    int g_fontHeight = 0;

    const D3DCOLOR COLOR_PANEL =
        D3DCOLOR_ARGB(220, 15, 15, 15);

    const D3DCOLOR COLOR_BUTTON =
        D3DCOLOR_ARGB(220, 45, 45, 45);

    const D3DCOLOR COLOR_BUTTON_HOVER =
        D3DCOLOR_ARGB(240, 80, 80, 80);

    const D3DCOLOR COLOR_SELECTED =
        D3DCOLOR_ARGB(240, 70, 70, 70);

    const D3DCOLOR COLOR_TEXT =
        D3DCOLOR_ARGB(255, 255, 255, 255);

    const D3DCOLOR COLOR_MUTED =
        D3DCOLOR_ARGB(255, 180, 180, 180);

    float UIX(float x)
    {
        return g_uiOffsetX + (x * g_uiScale);
    }

    float UIY(float y)
    {
        return g_uiOffsetY + (y * g_uiScale);
    }

    RECT ToScreenRect(const RECT& logicalRect)
    {
        RECT screenRect =
        {
            static_cast<LONG>(
                std::lround(UIX(
                    static_cast<float>(logicalRect.left)
                ))
            ),
            static_cast<LONG>(
                std::lround(UIY(
                    static_cast<float>(logicalRect.top)
                ))
            ),
            static_cast<LONG>(
                std::lround(UIX(
                    static_cast<float>(logicalRect.right)
                ))
            ),
            static_cast<LONG>(
                std::lround(UIY(
                    static_cast<float>(logicalRect.bottom)
                ))
            )
        };

        return screenRect;
    }

    void CreateFont(LPDIRECT3DDEVICE9 device)
    {
        const int desiredFontHeight =
            std::max(
                8,
                static_cast<int>(
                    std::lround(18.0f * g_uiScale)
                    )
            );

        if (g_font &&
            g_fontHeight == desiredFontHeight)
        {
            return;
        }

        if (g_font)
        {
            g_font->Release();
            g_font = nullptr;
            g_fontHeight = 0;
        }

        HRESULT hr = D3DXCreateFontA(
            device,
            desiredFontHeight,
            0,
            FW_BOLD,
            1,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Arial",
            &g_font
        );

        if (FAILED(hr))
        {
            Logger::Error(
                "MusicPlayerUI: failed to create D3DX font."
            );

            return;
        }

        g_fontHeight = desiredFontHeight;
    }

    void LoadFancyMiniPlayerTexture(
        LPDIRECT3DDEVICE9 device)
    {
        if (g_fancyMiniPlayerTexture)
            return;

        const std::string path =
            Paths::GetModRoot() +
            "\\data\\ui\\fancyminiplayer.png";

        HRESULT hr =
            D3DXCreateTextureFromFileExA(
                device,
                path.c_str(),
                D3DX_DEFAULT,
                D3DX_DEFAULT,
                1,
                0,
                D3DFMT_A8R8G8B8,
                D3DPOOL_MANAGED,
                D3DX_FILTER_LINEAR,
                D3DX_FILTER_LINEAR,
                0,
                nullptr,
                nullptr,
                &g_fancyMiniPlayerTexture
            );

        if (FAILED(hr))
        {
            Logger::Error(
                "MusicPlayerUI: failed to load fancyminiplayer.png."
            );

            return;
        }

        Logger::Info(
            "MusicPlayerUI: fancyminiplayer.png loaded."
        );
    }

    bool LoadUITexture(
        LPDIRECT3DDEVICE9 device,
        const std::string& relativePath,
        LPDIRECT3DTEXTURE9& texture)
    {
        if (texture)
            return true;

        const std::string path =
            Paths::GetModRoot() +
            "\\data\\ui\\" +
            relativePath;

        HRESULT hr =
            D3DXCreateTextureFromFileExA(
                device,
                path.c_str(),
                D3DX_DEFAULT,
                D3DX_DEFAULT,
                1,
                0,
                D3DFMT_A8R8G8B8,
                D3DPOOL_MANAGED,
                D3DX_FILTER_LINEAR,
                D3DX_FILTER_LINEAR,
                0,
                nullptr,
                nullptr,
                &texture
            );

        if (FAILED(hr))
        {
            texture = nullptr;

            Logger::Error(
                ("MusicPlayerUI: failed to load texture: " +
                    relativePath).c_str()
            );

            return false;
        }

        return true;
    }

    void LoadPlayerTextures(LPDIRECT3DDEVICE9 device)
    {
        LoadUITexture(
            device,
            "playertextures/Previous.png",
            g_previousTexture
        );

        LoadUITexture(
            device,
            "playertextures/Previous_H.png",
            g_previousHoverTexture
        );

        LoadUITexture(
            device,
            "playertextures/Play.png",
            g_playTexture
        );

        LoadUITexture(
            device,
            "playertextures/Play_H.png",
            g_playHoverTexture
        );

        LoadUITexture(
            device,
            "playertextures/Pause.png",
            g_pauseTexture
        );

        LoadUITexture(
            device,
            "playertextures/Pause_H.png",
            g_pauseHoverTexture
        );

        LoadUITexture(
            device,
            "playertextures/Next.png",
            g_nextTexture
        );

        LoadUITexture(
            device,
            "playertextures/Next_H.png",
            g_nextHoverTexture
        );

        LoadUITexture(
            device,
            "playertextures/Shuffle.png",
            g_shuffleTexture
        );

        LoadUITexture(
            device,
            "playertextures/Shuffle_H_ON.png",
            g_shuffleOnTexture
        );

        LoadUITexture(
            device,
            "playertextures/Mute.png",
            g_muteTexture
        );

        LoadUITexture(
            device,
            "playertextures/Mute_H_ON.png",
            g_muteOnTexture
        );

        LoadUITexture(
            device,
            "cursor.png",
            g_cursorTexture
        );
    }

    void CreateMiniFont(
        LPDIRECT3DDEVICE9 device)
    {
        const float logicalFontHeight =
            g_miniPlayerSkin == MiniPlayerSkin::SIMPLE
            ? 15.0f
            : 13.0f;

        const int desiredFontHeight =
            std::max(
                8,
                static_cast<int>(
                    std::lround(
                        logicalFontHeight * g_uiScale
                    )
                    )
            );

        if (g_miniFont &&
            g_miniFontHeight == desiredFontHeight)
        {
            return;
        }

        if (g_miniFont)
        {
            g_miniFont->Release();
            g_miniFont = nullptr;
        }

        HRESULT hr =
            D3DXCreateFontA(
                device,
                desiredFontHeight,
                0,
                FW_NORMAL,
                1,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                DEFAULT_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                "Arial",
                &g_miniFont
            );

        if (FAILED(hr))
        {
            Logger::Error(
                "MusicPlayerUI: failed to create mini-player font."
            );

            return;
        }

        g_miniFontHeight =
            desiredFontHeight;
    }

    void DrawTexture(
        LPDIRECT3DDEVICE9 device,
        LPDIRECT3DTEXTURE9 texture,
        const RECT& logicalRect)
    {
        if (!texture)
            return;

        const RECT screenRect =
            ToScreenRect(logicalRect);

        struct Vertex
        {
            float x;
            float y;
            float z;
            float rhw;
            D3DCOLOR color;
            float u;
            float v;
        };

        const D3DCOLOR white =
            D3DCOLOR_ARGB(
                255,
                255,
                255,
                255
            );

        Vertex vertices[4] =
        {
            {
                (float)screenRect.left,
                (float)screenRect.top,
                0.0f,
                1.0f,
                white,
                0.0f,
                0.0f
            },

            {
                (float)screenRect.right,
                (float)screenRect.top,
                0.0f,
                1.0f,
                white,
                1.0f,
                0.0f
            },

            {
                (float)screenRect.right,
                (float)screenRect.bottom,
                0.0f,
                1.0f,
                white,
                1.0f,
                1.0f
            },

            {
                (float)screenRect.left,
                (float)screenRect.bottom,
                0.0f,
                1.0f,
                white,
                0.0f,
                1.0f
            }
        };

        device->SetTexture(
            0,
            texture
        );

        device->SetTextureStageState(
            0,
            D3DTSS_COLOROP,
            D3DTOP_MODULATE
        );

        device->SetTextureStageState(
            0,
            D3DTSS_COLORARG1,
            D3DTA_TEXTURE
        );

        device->SetTextureStageState(
            0,
            D3DTSS_COLORARG2,
            D3DTA_DIFFUSE
        );

        device->SetTextureStageState(
            0,
            D3DTSS_ALPHAOP,
            D3DTOP_MODULATE
        );

        device->SetTextureStageState(
            0,
            D3DTSS_ALPHAARG1,
            D3DTA_TEXTURE
        );

        device->SetTextureStageState(
            0,
            D3DTSS_ALPHAARG2,
            D3DTA_DIFFUSE
        );

        device->SetSamplerState(
            0,
            D3DSAMP_MINFILTER,
            D3DTEXF_LINEAR
        );

        device->SetSamplerState(
            0,
            D3DSAMP_MAGFILTER,
            D3DTEXF_LINEAR
        );

        device->SetRenderState(
            D3DRS_ALPHABLENDENABLE,
            TRUE
        );

        device->SetRenderState(
            D3DRS_SRCBLEND,
            D3DBLEND_SRCALPHA
        );

        device->SetRenderState(
            D3DRS_DESTBLEND,
            D3DBLEND_INVSRCALPHA
        );

        device->SetRenderState(
            D3DRS_SCISSORTESTENABLE,
            FALSE
        );

        device->SetFVF(
            D3DFVF_XYZRHW |
            D3DFVF_DIFFUSE |
            D3DFVF_TEX1
        );

        device->DrawPrimitiveUP(
            D3DPT_TRIANGLEFAN,
            2,
            vertices,
            sizeof(Vertex)
        );

        device->SetTexture(
            0,
            nullptr
        );
    }

    RECT GetCenteredTextureRect(
        const RECT& area,
        LONG size)
    {
        const LONG centerX =
            (area.left + area.right) / 2;

        const LONG centerY =
            (area.top + area.bottom) / 2;

        return
        {
            centerX - size / 2,
            centerY - size / 2,
            centerX + size / 2,
            centerY + size / 2
        };
    }

    float EaseOutCubic(float t);

    void UpdateMiniPlayerAnimation(
        float dt)
    {
        if (!g_miniAnimationActive)
            return;

        g_miniAnimationElapsed += dt;

        float t =
            g_miniAnimationElapsed /
            MINI_ANIMATION_DURATION;

        if (t >= 1.0f)
            t = 1.0f;

        const float eased =
            EaseOutCubic(t);

        g_miniScreenX =
            g_miniAnimationStartX +
            (
                g_miniAnimationEndX -
                g_miniAnimationStartX
                ) * eased;

        if (t >= 1.0f)
        {
            g_miniScreenX =
                g_miniAnimationEndX;

            g_miniAnimationActive =
                false;
        }
    }

    void DrawMiniPosition(
        LPDIRECT3DDEVICE9 device,
        const RECT& rect)
    {
        size_t position = 0;
        size_t total = 0;

        MusicManager::GetCurrentPlaylistPosition(
            position,
            total
        );

        char text[32];

        if (position > 0)
        {
            sprintf_s(
                text,
                "%zu of %zu",
                position,
                total
            );
        }
        else
        {
            sprintf_s(
                text,
                "-- of %zu",
                total
            );
        }

        if (!g_miniFont)
            return;

        RECT screenRect =
            ToScreenRect(rect);

        g_miniFont->DrawTextA(
            nullptr,
            text,
            -1,
            &screenRect,
            DT_LEFT |
            DT_SINGLELINE,
            D3DCOLOR_ARGB(
                255,
                150,
                160,
                165
            )
        );
    }

    void DrawMarqueeFadePair(
        LPDIRECT3DDEVICE9 device,
        const RECT& clipRect,
        D3DCOLOR fadeColor);

    void DrawScrollingText(
        LPDIRECT3DDEVICE9 device,
        LPD3DXFONT font,
        const char* text,
        const RECT& clipRect,
        MarqueeState& state,
        D3DCOLOR textColor,
        D3DCOLOR fadeColor,
        float dt)
    {
        if (!font)
            return;

        if (!text)
            text = "";

        if (state.text != text)
        {
            state.text = text;
            state.offset = 0.0f;
            state.holdTime =
                MARQUEE_HOLD;
        }

        RECT screenClip =
            ToScreenRect(clipRect);

        RECT measureRect =
        {
            0,
            0,
            2000,
            100
        };

        font->DrawTextA(
            nullptr,
            text,
            -1,
            &measureRect,
            DT_CALCRECT |
            DT_SINGLELINE,
            textColor
        );

        const LONG textWidth =
            measureRect.right -
            measureRect.left;

        const LONG availableWidth =
            screenClip.right -
            screenClip.left;

        if (textWidth <= availableWidth)
        {
            font->DrawTextA(
                nullptr,
                text,
                -1,
                &screenClip,
                DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE,
                textColor
            );

            return;
        }

        if (state.holdTime > 0.0f)
        {
            state.holdTime -= dt;
        }
        else
        {
            state.offset +=
                MARQUEE_SPEED * dt;
        }

        const float cycleWidth =
            textWidth + MARQUEE_GAP;

        if (state.offset >= cycleWidth)
        {
            state.offset =
                std::fmod(
                    state.offset,
                    cycleWidth
                );
        }

        device->SetRenderState(
            D3DRS_SCISSORTESTENABLE,
            TRUE
        );

        device->SetScissorRect(
            &screenClip
        );

        RECT firstRect = screenClip;

        firstRect.left =
            screenClip.left -
            static_cast<LONG>(
                std::lround(
                    state.offset
                )
                );

        firstRect.right =
            firstRect.left +
            textWidth;

        font->DrawTextA(
            nullptr,
            text,
            -1,
            &firstRect,
            DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE,
            textColor
        );

        RECT secondRect =
            firstRect;

        secondRect.left +=
            static_cast<LONG>(
                std::lround(
                    cycleWidth
                )
                );

        secondRect.right =
            secondRect.left +
            textWidth;

        font->DrawTextA(
            nullptr,
            text,
            -1,
            &secondRect,
            DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE,
            textColor
        );

        device->SetRenderState(
            D3DRS_SCISSORTESTENABLE,
            FALSE
        );

        DrawMarqueeFadePair(
            device,
            clipRect,
            fadeColor
        );
    }

    void DrawHorizontalFade(
        LPDIRECT3DDEVICE9 device,
        const RECT& rect,
        bool leftFade,
        D3DCOLOR baseColor)
    {
        const RECT screenRect =
            ToScreenRect(rect);

        struct Vertex
        {
            float x, y, z, rhw;
            D3DCOLOR color;
        };

        D3DCOLOR transparent =
            D3DCOLOR_ARGB(
                0,
                GetRValue(baseColor),
                GetGValue(baseColor),
                GetBValue(baseColor)
            );

        D3DCOLOR opaque =
            D3DCOLOR_ARGB(
                255,
                GetRValue(baseColor),
                GetGValue(baseColor),
                GetBValue(baseColor)
            );

        Vertex vertices[4];

        if (leftFade)
        {
            vertices[0] =
            {
                (float)screenRect.left,
                (float)screenRect.top,
                0, 1,
                opaque
            };

            vertices[1] =
            {
                (float)screenRect.right,
                (float)screenRect.top,
                0, 1,
                transparent
            };

            vertices[2] =
            {
                (float)screenRect.right,
                (float)screenRect.bottom,
                0, 1,
                transparent
            };

            vertices[3] =
            {
                (float)screenRect.left,
                (float)screenRect.bottom,
                0, 1,
                opaque
            };
        }
        else
        {
            vertices[0] =
            {
                (float)screenRect.left,
                (float)screenRect.top,
                0, 1,
                transparent
            };

            vertices[1] =
            {
                (float)screenRect.right,
                (float)screenRect.top,
                0, 1,
                opaque
            };

            vertices[2] =
            {
                (float)screenRect.right,
                (float)screenRect.bottom,
                0, 1,
                opaque
            };

            vertices[3] =
            {
                (float)screenRect.left,
                (float)screenRect.bottom,
                0, 1,
                transparent
            };
        }

        device->SetFVF(
            D3DFVF_XYZRHW |
            D3DFVF_DIFFUSE
        );

        device->DrawPrimitiveUP(
            D3DPT_TRIANGLEFAN,
            2,
            vertices,
            sizeof(Vertex)
        );
    }

    void DrawMarqueeFadePair(
        LPDIRECT3DDEVICE9 device,
        const RECT& clipRect,
        D3DCOLOR fadeColor)
    {
        RECT leftFade =
        {
            clipRect.left,
            clipRect.top,
            clipRect.left + MARQUEE_FADE_WIDTH,
            clipRect.bottom
        };

        RECT rightFade =
        {
            clipRect.right - MARQUEE_FADE_WIDTH,
            clipRect.top,
            clipRect.right,
            clipRect.bottom
        };

        DrawHorizontalFade(
            device,
            leftFade,
            true,
            fadeColor
        );

        DrawHorizontalFade(
            device,
            rightFade,
            false,
            fadeColor
        );
    }

    void SetMiniPlayerTargetVisible(
        bool visible)
    {
        g_miniWantedVisible = visible;
    }

    void ToggleMenu()
    {
        g_menuOpen = !g_menuOpen;

        SetMiniPlayerTargetVisible(
            g_enabled && !g_menuOpen
        );

        if (g_menuOpen)
        {
            g_menuFocus = MenuFocus::TABS;
            g_menuSelection = 0;
            g_musicScrollOffset = 0;
            g_settingsScrollOffset = 0;
        }

        Logger::Info(
            g_menuOpen
            ? "MusicPlayerUI: main menu opened."
            : "MusicPlayerUI: main menu closed."
        );
    }

    void CloseMenu()
    {
        if (!g_menuOpen)
            return;

        g_menuOpen = false;

        SetMiniPlayerTargetVisible(
            g_enabled
        );

        Logger::Info(
            "MusicPlayerUI: main menu closed."
        );
    }

    bool IsKeyPressed(int virtualKey)
    {
        static bool previousState[256] = {};

        if (virtualKey < 0 || virtualKey >= 256)
            return false;

        const bool currentState =
            (GetAsyncKeyState(virtualKey) & 0x8000) != 0;

        const bool pressed =
            currentState && !previousState[virtualKey];

        previousState[virtualKey] = currentState;

        return pressed;
    }

    bool IsMouseButtonPressed()
    {
        static bool previousState = false;

        const bool currentState =
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        const bool pressed =
            currentState && !previousState;

        previousState = currentState;

        return pressed;
    }

    bool IsMouseButtonDown()
    {
        return
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    }

    void UpdateMouseScrollbarDrag()
    {
        if (!g_menuOpen)
        {
            g_musicScrollbarDragging = false;
            g_settingsScrollbarDragging = false;
            return;
        }

        if (!IsMouseButtonDown())
        {
            g_musicScrollbarDragging = false;
            g_settingsScrollbarDragging = false;
            return;
        }

        POINT mouse = {};

        if (!GetMousePosition(mouse))
            return;

        if (g_musicScrollbarDragging)
        {
            ScrollMusicFromScrollbarY(mouse.y);
            return;
        }

        if (g_settingsScrollbarDragging)
        {
            ScrollSettingsFromScrollbarY(mouse.y);
            return;
        }
    }

    bool GetMousePosition(POINT& point)
    {
        HWND hwnd = GetForegroundWindow();

        if (!hwnd)
            return false;

        if (!GetCursorPos(&point))
            return false;

        if (!ScreenToClient(hwnd, &point))
            return false;

        if (g_uiScale <= 0.0f)
            return false;

        point.x = static_cast<LONG>(
            std::lround(
                (static_cast<float>(point.x) -
                    g_uiOffsetX) /
                g_uiScale
            )
            );

        point.y = static_cast<LONG>(
            std::lround(
                (static_cast<float>(point.y) -
                    g_uiOffsetY) /
                g_uiScale
            )
            );

        return true;
    }

    std::string GetKeyDisplayName(int virtualKey)
    {
        UINT scanCode =
            MapVirtualKeyA(
                static_cast<UINT>(virtualKey),
                MAPVK_VK_TO_VSC
            );

        if (scanCode != 0)
        {
            LONG lParam =
                static_cast<LONG>(scanCode << 16);

            char keyName[64] = {};

            if (GetKeyNameTextA(
                lParam,
                keyName,
                sizeof(keyName)
            ) > 0)
            {
                return keyName;
            }
        }

        char fallback[16] = {};

        sprintf_s(
            fallback,
            "0x%02X",
            virtualKey
        );

        return fallback;
    }

    float GetSafeGameDeltaTime()
    {
        float dt =
            GameStateManager::GetGameDeltaTime();

        if (!std::isfinite(dt) || dt <= 0.0f)
            return 1.0f / 60.0f;

        if (dt > 0.05f)
            dt = 0.05f;

        return dt;
    }

    void DrawText(
        LPDIRECT3DDEVICE9 device,
        const char* text,
        RECT rect,
        DWORD format,
        D3DCOLOR color = COLOR_TEXT
    )
    {
        if (g_menuOpen && !g_font)
            return;

        if (!g_menuOpen)
        {
            if (g_miniPlayerSkin ==
                MiniPlayerSkin::IPOD_CLASSIC_5)
            {
                if (!g_miniFont ||
                    !g_fancyMiniPlayerTexture)
                {
                    return;
                }
            }
            else
            {
                if (!g_font)
                    return;
            }
        }

        RECT screenRect =
            ToScreenRect(rect);

        g_font->DrawTextA(
            nullptr,
            text,
            -1,
            &screenRect,
            format,
            color
        );
    }

    void DrawRect(
        LPDIRECT3DDEVICE9 device,
        const RECT& rect,
        D3DCOLOR color
    )
    {
        const RECT screenRect =
            ToScreenRect(rect);

        struct Vertex
        {
            float x;
            float y;
            float z;
            float rhw;
            D3DCOLOR color;
        };

        Vertex vertices[4] =
        {
            {
                static_cast<float>(screenRect.left),
                static_cast<float>(screenRect.top),
                0.0f,
                1.0f,
                color
            },
            {
                static_cast<float>(screenRect.right),
                static_cast<float>(screenRect.top),
                0.0f,
                1.0f,
                color
            },
            {
                static_cast<float>(screenRect.right),
                static_cast<float>(screenRect.bottom),
                0.0f,
                1.0f,
                color
            },
            {
                static_cast<float>(screenRect.left),
                static_cast<float>(screenRect.bottom),
                0.0f,
                1.0f,
                color
            }
        };

        device->SetFVF(
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE
        );

        device->DrawPrimitiveUP(
            D3DPT_TRIANGLEFAN,
            2,
            vertices,
            sizeof(Vertex)
        );
    }

    void DrawScrollbar(
        LPDIRECT3DDEVICE9 device,
        const RECT& track,
        const RECT& thumb,
        bool visible,
        bool hovered)
    {
        if (!visible)
            return;

        DrawRect(
            device,
            track,
            D3DCOLOR_ARGB(90, 80, 80, 80)
        );

        DrawRect(
            device,
            thumb,
            hovered
            ? D3DCOLOR_ARGB(230, 170, 170, 170)
            : D3DCOLOR_ARGB(190, 120, 120, 120)
        );
    }

    void DrawButton(
        LPDIRECT3DDEVICE9 device,
        const RECT& rect,
        const char* label,
        bool hovered,
        bool selected = false
    )
    {
        D3DCOLOR color = COLOR_BUTTON;

        if (selected)
            color = COLOR_SELECTED;
        else if (hovered)
            color = COLOR_BUTTON_HOVER;

        DrawRect(device, rect, color);

        DrawText(
            device,
            label,
            rect,
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE
        );
    }

    void DrawPlayerTextureButton(
        LPDIRECT3DDEVICE9 device,
        const RECT& rect,
        LPDIRECT3DTEXTURE9 normalTexture,
        LPDIRECT3DTEXTURE9 hoverTexture,
        bool hovered)
    {
        LPDIRECT3DTEXTURE9 texture =
            hovered && hoverTexture
            ? hoverTexture
            : normalTexture;

        if (!texture)
            return;

        DrawTexture(
            device,
            texture,
            rect
        );
    }

    void DrawTogglePlayerTextureButton(
        LPDIRECT3DDEVICE9 device,
        const RECT& rect,
        LPDIRECT3DTEXTURE9 normalTexture,
        LPDIRECT3DTEXTURE9 hoverTexture,
        LPDIRECT3DTEXTURE9 onTexture,
        bool hovered,
        bool enabled)
    {
        LPDIRECT3DTEXTURE9 texture = normalTexture;

        if (enabled && onTexture)
        {
            texture = onTexture;
        }
        else if (hovered && hoverTexture)
        {
            texture = hoverTexture;
        }
        else if (hovered && onTexture)
        {
            texture = onTexture;
        }

        if (!texture)
            return;

        DrawTexture(
            device,
            texture,
            rect
        );
    }

    void RenderCursor(
        LPDIRECT3DDEVICE9 device)
    {
        if (!g_cursorTexture)
            return;

        POINT mouse = {};

        if (!GetMousePosition(mouse))
            return;

        constexpr LONG CURSOR_SIZE = 32;

        RECT cursorRect =
        {
            mouse.x,
            mouse.y,
            mouse.x + CURSOR_SIZE,
            mouse.y + CURSOR_SIZE
        };

        DrawTexture(
            device,
            g_cursorTexture,
            cursorRect
        );
    }

    void DrawOptionValue(
        LPDIRECT3DDEVICE9 device,
        const RECT& valueRect,
        const char* value,
        bool selected)
    {
        if (!selected)
        {
            DrawText(
                device,
                value,
                valueRect,
                DT_CENTER |
                DT_VCENTER |
                DT_SINGLELINE
            );

            return;
        }

        constexpr LONG ARROW_WIDTH = 20;

        RECT leftArrowRect =
        {
            valueRect.left,
            valueRect.top,
            valueRect.left + ARROW_WIDTH,
            valueRect.bottom
        };

        RECT valueTextRect =
        {
            valueRect.left + ARROW_WIDTH,
            valueRect.top,
            valueRect.right - ARROW_WIDTH,
            valueRect.bottom
        };

        RECT rightArrowRect =
        {
            valueRect.right - ARROW_WIDTH,
            valueRect.top,
            valueRect.right,
            valueRect.bottom
        };

        DrawText(
            device,
            "<",
            leftArrowRect,
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE
        );

        DrawText(
            device,
            value,
            valueTextRect,
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE
        );

        DrawText(
            device,
            ">",
            rightArrowRect,
            DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE
        );
    }

    float EaseOutCubic(float t)
    {
        const float inv = 1.0f - t;
        return 1.0f -
            inv * inv * inv;
    }

    const char* GetContextName(TrackContext context)
    {
        switch (context)
        {
        case TrackContext::FE:
            return "FE";

        case TrackContext::GAMEPLAY:
            return "RACE";

        case TrackContext::BOTH:
            return "ALL";

        default:
            return "UNKNOWN";
        }
    }

    TrackContext GetNextContext(TrackContext context)
    {
        switch (context)
        {
        case TrackContext::FE:
            return TrackContext::GAMEPLAY;

        case TrackContext::GAMEPLAY:
            return TrackContext::BOTH;

        case TrackContext::BOTH:
        default:
            return TrackContext::FE;
        }
    }

    TrackContext GetPreviousContext(TrackContext context)
    {
        switch (context)
        {
        case TrackContext::FE:
            return TrackContext::BOTH;

        case TrackContext::GAMEPLAY:
            return TrackContext::FE;

        case TrackContext::BOTH:
        default:
            return TrackContext::GAMEPLAY;
        }
    }

    void EnsureMusicSelectionVisible()
    {
        const auto& tracks = Playlist::GetTracks();

        if (tracks.empty())
        {
            g_musicScrollOffset = 0;
            g_menuSelection = 0;
            return;
        }

        const int totalTracks =
            static_cast<int>(tracks.size());

        const int calculatedVisibleRows =
            static_cast<int>(
                (MUSIC_LIST_BOTTOM - MUSIC_LIST_TOP) /
                MUSIC_ROW_HEIGHT
                );

        const int visibleRows =
            calculatedVisibleRows > 0
            ? calculatedVisibleRows
            : 1;

        if (g_menuSelection < 0)
            g_menuSelection = 0;

        if (g_menuSelection >= totalTracks)
            g_menuSelection = totalTracks - 1;

        if (g_menuSelection < g_musicScrollOffset)
        {
            g_musicScrollOffset =
                g_menuSelection;
        }

        if (g_menuSelection >=
            g_musicScrollOffset + visibleRows)
        {
            g_musicScrollOffset =
                g_menuSelection - visibleRows + 1;
        }

        const int calculatedMaxOffset =
            totalTracks - visibleRows;

        const int maxOffset =
            calculatedMaxOffset > 0
            ? calculatedMaxOffset
            : 0;

        if (g_musicScrollOffset > maxOffset)
            g_musicScrollOffset = maxOffset;
    }

    void ScrollMusicList(int amount)
    {
        const auto& tracks =
            Playlist::GetTracks();

        if (tracks.empty())
        {
            g_musicScrollOffset = 0;
            return;
        }

        const int calculatedVisibleRows =
            static_cast<int>(
                (MUSIC_LIST_BOTTOM - MUSIC_LIST_TOP) /
                MUSIC_ROW_HEIGHT
                );

        const int visibleRows =
            calculatedVisibleRows > 0
            ? calculatedVisibleRows
            : 1;

        const int maxOffset =
            std::max(
                0,
                static_cast<int>(tracks.size()) -
                visibleRows
            );

        g_musicScrollOffset += amount;

        if (g_musicScrollOffset < 0)
            g_musicScrollOffset = 0;

        if (g_musicScrollOffset > maxOffset)
            g_musicScrollOffset = maxOffset;

        // Keep keyboard selection inside the visible region.
        if (g_menuSelection < g_musicScrollOffset)
        {
            g_menuSelection = g_musicScrollOffset;
        }

        if (g_menuSelection >=
            g_musicScrollOffset + visibleRows)
        {
            g_menuSelection =
                g_musicScrollOffset + visibleRows - 1;
        }

        if (g_menuSelection >=
            static_cast<int>(tracks.size()))
        {
            g_menuSelection =
                static_cast<int>(tracks.size()) - 1;
        }
    }

    void ScrollMusicFromScrollbarY(LONG mouseY)
    {
        const int maxOffset =
            GetMusicMaxScrollOffset();

        if (maxOffset <= 0)
            return;

        const LONG trackTop =
            g_musicScrollbarTrack.top;

        const LONG trackBottom =
            g_musicScrollbarTrack.bottom;

        const LONG thumbHeight =
            g_musicScrollbarThumb.bottom -
            g_musicScrollbarThumb.top;

        const LONG movableHeight =
            (trackBottom - trackTop) -
            thumbHeight;

        if (movableHeight <= 0)
            return;

        LONG thumbTop =
            mouseY -
            (
                g_musicScrollbarDragging
                ? g_musicScrollbarDragOffset
                : thumbHeight / 2
                );

        if (thumbTop < trackTop)
            thumbTop = trackTop;

        const LONG maxThumbTop =
            trackBottom - thumbHeight;

        if (thumbTop > maxThumbTop)
            thumbTop = maxThumbTop;

        const float ratio =
            static_cast<float>(
                thumbTop - trackTop
                ) /
            static_cast<float>(
                movableHeight
                );

        const int offset =
            static_cast<int>(
                std::lround(
                    ratio * maxOffset
                )
                );

        SetMusicScrollOffset(offset);
    }

    void ScrollSettingsFromScrollbarY(LONG mouseY)
    {
        const int maxOffset =
            GetSettingsMaxScrollOffset();

        if (maxOffset <= 0)
            return;

        const LONG trackTop =
            g_settingsScrollbarTrack.top;

        const LONG trackBottom =
            g_settingsScrollbarTrack.bottom;

        const LONG thumbHeight =
            g_settingsScrollbarThumb.bottom -
            g_settingsScrollbarThumb.top;

        const LONG movableHeight =
            (trackBottom - trackTop) -
            thumbHeight;

        if (movableHeight <= 0)
            return;

        LONG thumbTop =
            mouseY -
            (
                g_settingsScrollbarDragging
                ? g_settingsScrollbarDragOffset
                : thumbHeight / 2
                );

        if (thumbTop < trackTop)
            thumbTop = trackTop;

        const LONG maxThumbTop =
            trackBottom - thumbHeight;

        if (thumbTop > maxThumbTop)
            thumbTop = maxThumbTop;

        const float ratio =
            static_cast<float>(
                thumbTop - trackTop
                ) /
            static_cast<float>(
                movableHeight
                );

        const int offset =
            static_cast<int>(
                std::lround(
                    ratio * maxOffset
                )
                );

        SetSettingsScrollOffset(offset);
    }

    void GetMenuTabRects(
        RECT& musicTab,
        RECT& settingsTab,
        RECT& creditsTab)
    {
        const LONG tabTop =
            g_mainMenuRect.top + 60;

        const LONG tabHeight = 35;
        const LONG tabWidth = 180;
        const LONG tabGap = 10;

        musicTab =
        {
            g_mainMenuRect.left + 15,
            tabTop,
            g_mainMenuRect.left + 15 + tabWidth,
            tabTop + tabHeight
        };

        settingsTab =
        {
            musicTab.right + tabGap,
            tabTop,
            musicTab.right + tabGap + tabWidth,
            tabTop + tabHeight
        };

        creditsTab =
        {
            settingsTab.right + tabGap,
            tabTop,
            settingsTab.right + tabGap + tabWidth,
            tabTop + tabHeight
        };
    }

    void EnsureSettingsSelectionVisible();

    void UpdateLayout(LPDIRECT3DDEVICE9 device)
    {
        D3DVIEWPORT9 viewport = {};

        if (FAILED(device->GetViewport(&viewport)))
            return;

        const LONG screenWidth =
            static_cast<LONG>(viewport.Width);

        const LONG screenHeight =
            static_cast<LONG>(viewport.Height);

        const float scaleX =
            static_cast<float>(screenWidth) /
            UI_BASE_WIDTH;

        const float scaleY =
            static_cast<float>(screenHeight) /
            UI_BASE_HEIGHT;

        g_uiScale =
            std::min(scaleX, scaleY) * UI_SCALE_FACTOR;

        g_uiOffsetX =
            (static_cast<float>(screenWidth) -
                UI_BASE_WIDTH * g_uiScale) * 0.5f;

        g_uiOffsetY =
            (static_cast<float>(screenHeight) -
                UI_BASE_HEIGHT * g_uiScale) * 0.5f;


        LONG miniWidth;
        LONG miniHeight;

        if (g_miniPlayerSkin ==
            MiniPlayerSkin::IPOD_CLASSIC_5)
        {
            miniWidth = IPOD_WIDTH;
            miniHeight = IPOD_HEIGHT;
        }
        else
        {
            miniWidth = PLAYER_WIDTH;
            miniHeight = PLAYER_HEIGHT;
        }

        const float miniWidthPx =
            miniWidth * g_uiScale;

        const float miniHeightPx =
            miniHeight * g_uiScale;

        const float miniTargetX =
            MINI_LEFT_MARGIN;

        const float miniTargetY =
            (
                static_cast<float>(screenHeight) -
                miniHeightPx
                ) * 0.5f;

        const float miniLogicalTargetX =
            (miniTargetX - g_uiOffsetX) /
            g_uiScale;

        const float miniLogicalTargetY =
            (miniTargetY - g_uiOffsetY) /
            g_uiScale;

        g_miniPlayerTargetRect =
        {
            static_cast<LONG>(
                std::lround(miniLogicalTargetX)
            ),

            static_cast<LONG>(
                std::lround(miniLogicalTargetY)
            ),

            static_cast<LONG>(
                std::lround(
                    miniLogicalTargetX +
                    miniWidth
                )
            ),

            static_cast<LONG>(
                std::lround(
                    miniLogicalTargetY +
                    miniHeight
                )
            )
        };

        const float targetScreenX =
            UIX(
                static_cast<float>(
                    g_miniPlayerTargetRect.left
                    )
            );

        const bool viewportChanged =
            g_miniLastScreenWidth != screenWidth ||
            g_miniLastScreenHeight != screenHeight;

        g_miniLastScreenWidth = screenWidth;
        g_miniLastScreenHeight = screenHeight;

        if (!g_miniVisibilityInitialized)
        {
            g_miniScreenX =
                g_miniWantedVisible
                ? targetScreenX
                : static_cast<float>(screenWidth) +
                MINI_EXIT_MARGIN;

            g_miniTargetScreenX =
                g_miniScreenX;

            g_miniVisibilityInitialized =
                true;

            g_miniAnimationActive =
                false;
        }
        else if (viewportChanged)
        {
            // Resolution/viewport changes must not trigger visibility animation.
            const float desiredScreenX =
                g_miniWantedVisible
                ? targetScreenX
                : static_cast<float>(screenWidth) +
                MINI_EXIT_MARGIN;

            g_miniScreenX =
                desiredScreenX;

            g_miniTargetScreenX =
                desiredScreenX;

            g_miniAnimationElapsed = 0.0f;
            g_miniAnimationActive = false;
        }
        else
        {
            const float desiredScreenX =
                g_miniWantedVisible
                ? targetScreenX
                : static_cast<float>(screenWidth) +
                MINI_EXIT_MARGIN;

            if (desiredScreenX !=
                g_miniTargetScreenX)
            {
                g_miniAnimationElapsed = 0.0f;
                g_miniAnimationActive = true;

                if (g_miniWantedVisible)
                {
                    g_miniAnimationStartX =
                        -miniWidthPx - MINI_LEFT_MARGIN;

                    g_miniAnimationEndX =
                        targetScreenX;
                }
                else
                {
                    // Every HIDE animation starts from the
                    // current position and exits to the left.
                    g_miniAnimationStartX =
                        g_miniScreenX;

                    g_miniAnimationEndX =
                        -miniWidthPx -
                        MINI_EXIT_MARGIN;
                }

                g_miniScreenX =
                    g_miniAnimationStartX;

                g_miniTargetScreenX =
                    desiredScreenX;
            }
        }

        const float animatedLogicalX =
            (
                g_miniScreenX -
                g_uiOffsetX
                ) / g_uiScale;

        g_playerRect =
        {
            static_cast<LONG>(
                std::lround(animatedLogicalX)
            ),

            g_miniPlayerTargetRect.top,

            static_cast<LONG>(
                std::lround(
                    animatedLogicalX +
                    miniWidth
                )
            ),

            g_miniPlayerTargetRect.bottom
        };

        if (g_miniPlayerSkin ==
            MiniPlayerSkin::SIMPLE)
        {
            const LONG left =
                g_playerRect.left;

            const LONG top =
                g_playerRect.top;

            const LONG totalButtonWidth =
                (BUTTON_SIZE * 3) +
                (BUTTON_GAP * 2);

            const LONG buttonStartX =
                left +
                (PLAYER_WIDTH -
                    totalButtonWidth) / 2;

            const LONG buttonY =
                top + 100;

            g_previousButton =
            {
                buttonStartX,
                buttonY,
                buttonStartX + BUTTON_SIZE,
                buttonY + BUTTON_SIZE
            };

            g_playPauseButton =
            {
                buttonStartX +
                    BUTTON_SIZE +
                    BUTTON_GAP,

                buttonY,

                buttonStartX +
                    (BUTTON_SIZE * 2) +
                    BUTTON_GAP,

                buttonY + BUTTON_SIZE
            };

            g_nextButton =
            {
                buttonStartX +
                    (BUTTON_SIZE * 2) +
                    (BUTTON_GAP * 2),

                buttonY,

                buttonStartX +
                    (BUTTON_SIZE * 3) +
                    (BUTTON_GAP * 2),

                buttonY + BUTTON_SIZE
            };

            g_menuButton =
            {
                g_playerRect.right - 80 - 10,
                g_playerRect.top + 5,
                g_playerRect.right - 10,
                g_playerRect.top + 35
            };
        }
        else
        {
            UpdateFancyMiniPlayerHitboxes(
                g_playerRect
            );
        }

        const LONG menuWidth = 700;
        const LONG menuHeight = 500;

        const LONG menuLeft =
            static_cast<LONG>(
                (UI_BASE_WIDTH - menuWidth) / 2.0f
                );

        const LONG menuTop =
            static_cast<LONG>(
                (UI_BASE_HEIGHT - menuHeight) / 2.0f
                );

        g_mainMenuRect =
        {
            menuLeft,
            menuTop,
            menuLeft + menuWidth,
            menuTop + menuHeight
        };

        const LONG closeButtonSize = 35;

        g_closeButton =
        {
            g_mainMenuRect.right - closeButtonSize - 10,
            g_mainMenuRect.top + 10,
            g_mainMenuRect.right - 10,
            g_mainMenuRect.top + 10 + closeButtonSize
        };

        if (g_uiStateBlock)
        {
            g_uiStateBlock->Release();
            g_uiStateBlock = nullptr;
        }

        g_musicRowRects.clear();
        g_musicContextRects.clear();

        const auto& tracks = Playlist::GetTracks();

        const int calculatedVisibleRows =
            static_cast<int>(
                (MUSIC_LIST_BOTTOM - MUSIC_LIST_TOP) /
                MUSIC_ROW_HEIGHT
                );

        const int visibleRows =
            calculatedVisibleRows > 0
            ? calculatedVisibleRows
            : 1;

        const LONG contentLeft =
            g_mainMenuRect.left + 25;

        const LONG contentRight =
            g_mainMenuRect.right - 25;

        const LONG contextWidth = 85;

        for (int row = 0; row < visibleRows; ++row)
        {
            RECT rowRect =
            {
                contentLeft,
                g_mainMenuRect.top +
                    MUSIC_LIST_TOP +
                    row * MUSIC_ROW_HEIGHT,
                contentRight,
                g_mainMenuRect.top +
                    MUSIC_LIST_TOP +
                    (row + 1) * MUSIC_ROW_HEIGHT
            };

            g_musicRowRects.push_back(rowRect);

            RECT contextRect =
            {
                contentRight - contextWidth,
                rowRect.top + 2,
                contentRight - 2,
                rowRect.bottom - 2
            };

            g_musicContextRects.push_back(contextRect);
        }

        (void)tracks;

        // Only the active tab is allowed to clamp/adjust g_menuSelection.
        // Previously this always called EnsureMusicSelectionVisible(), which
        // uses the music track count. Since the playlist currently has fewer
        // than 8 items, Settings selection 6/7 got clamped back to 5 every
        // frame during UpdateLayout().
        if (g_currentTab == MenuTab::MUSIC)
        {
            EnsureMusicSelectionVisible();
        }
        else if (g_currentTab == MenuTab::SETTINGS)
        {
            EnsureSettingsSelectionVisible();
        }

        // -------------------------------------------------
        // MUSIC SCROLLBAR
        // -------------------------------------------------

        const LONG scrollbarWidth = 8;
        const LONG scrollbarMargin = 6;

        g_musicScrollbarTrack =
        {
            contentRight + scrollbarMargin,
            g_mainMenuRect.top + MUSIC_LIST_TOP,
            contentRight + scrollbarMargin + scrollbarWidth,
            g_mainMenuRect.top + MUSIC_LIST_TOP +
                visibleRows * MUSIC_ROW_HEIGHT
        };

        const int musicItemCount =
            static_cast<int>(tracks.size());

        const int musicMaxOffset =
            std::max(
                0,
                musicItemCount - visibleRows
            );

        if (musicMaxOffset > 0)
        {
            const float ratio =
                static_cast<float>(visibleRows) /
                static_cast<float>(musicItemCount);

            const LONG trackHeight =
                g_musicScrollbarTrack.bottom -
                g_musicScrollbarTrack.top;

            const LONG thumbHeight =
                std::max(
                    18L,
                    static_cast<LONG>(
                        std::lround(
                            trackHeight * ratio
                        )
                        )
                );

            const LONG movableHeight =
                trackHeight - thumbHeight;

            const float scrollRatio =
                static_cast<float>(g_musicScrollOffset) /
                static_cast<float>(musicMaxOffset);

            const LONG thumbTop =
                g_musicScrollbarTrack.top +
                static_cast<LONG>(
                    std::lround(
                        movableHeight * scrollRatio
                    )
                    );

            g_musicScrollbarThumb =
            {
                g_musicScrollbarTrack.left,
                thumbTop,
                g_musicScrollbarTrack.right,
                thumbTop + thumbHeight
            };
        }
        else
        {
            g_musicScrollbarThumb =
                g_musicScrollbarTrack;
        }

        // -------------------------------------------------
        // SETTINGS SCROLLBAR
        // -------------------------------------------------

        const LONG settingsTrackTop =
            g_mainMenuRect.top + 120;

        const LONG settingsTrackBottom =
            settingsTrackTop +
            GetSettingsVisibleRows() * 42;

        const LONG settingsScrollbarWidth = 8;
        const LONG settingsScrollbarMargin = 6;

        g_settingsScrollbarTrack =
        {
            g_mainMenuRect.right - 40 +
                settingsScrollbarMargin,
            settingsTrackTop,
            g_mainMenuRect.right - 40 +
                settingsScrollbarWidth +
                settingsScrollbarMargin,
            settingsTrackBottom
        };

        const int settingsItemCount = 9;

        const int settingsVisibleRows =
            GetSettingsVisibleRows();

        const int settingsMaxOffset =
            std::max(
                0,
                settingsItemCount -
                settingsVisibleRows
            );

        if (settingsMaxOffset > 0)
        {
            const float ratio =
                static_cast<float>(settingsVisibleRows) /
                static_cast<float>(settingsItemCount);

            const LONG trackHeight =
                g_settingsScrollbarTrack.bottom -
                g_settingsScrollbarTrack.top;

            const LONG thumbHeight =
                std::max(
                    18L,
                    static_cast<LONG>(
                        std::lround(
                            trackHeight * ratio
                        )
                        )
                );

            const LONG movableHeight =
                trackHeight - thumbHeight;

            const float scrollRatio =
                static_cast<float>(
                    g_settingsScrollOffset
                    ) /
                static_cast<float>(
                    settingsMaxOffset
                    );

            const LONG thumbTop =
                g_settingsScrollbarTrack.top +
                static_cast<LONG>(
                    std::lround(
                        movableHeight * scrollRatio
                    )
                    );

            g_settingsScrollbarThumb =
            {
                g_settingsScrollbarTrack.left,
                thumbTop,
                g_settingsScrollbarTrack.right,
                thumbTop + thumbHeight
            };
        }
        else
        {
            g_settingsScrollbarThumb =
                g_settingsScrollbarTrack;
        }


    }

    int GetCurrentTabItemCount()
    {
        switch (g_currentTab)
        {
        case MenuTab::MUSIC:
            return static_cast<int>(
                Playlist::GetTracks().size()
                );

        case MenuTab::SETTINGS:
            return 9;

        case MenuTab::CREDITS:
            return 1;

        default:
            return 0;
        }
    }

    void EnsureSettingsSelectionVisible()
    {
        const int itemCount =
            GetCurrentTabItemCount();

        if (itemCount <= 0)
        {
            g_settingsScrollOffset = 0;
            g_menuSelection = 0;
            return;
        }

        const int VISIBLE_ROWS =
            GetSettingsVisibleRows();

        if (g_menuSelection < 0)
            g_menuSelection = 0;

        if (g_menuSelection >= itemCount)
            g_menuSelection = itemCount - 1;

        if (g_menuSelection < g_settingsScrollOffset)
        {
            g_settingsScrollOffset =
                g_menuSelection;
        }

        if (g_menuSelection >=
            g_settingsScrollOffset + VISIBLE_ROWS)
        {
            g_settingsScrollOffset =
                g_menuSelection - VISIBLE_ROWS + 1;
        }

        const int maxOffset =
            std::max(
                0,
                itemCount - VISIBLE_ROWS
            );

        if (g_settingsScrollOffset > maxOffset)
            g_settingsScrollOffset = maxOffset;

        if (g_settingsScrollOffset < 0)
            g_settingsScrollOffset = 0;
    }

    int GetMusicVisibleRows()
    {
        const int calculated =
            static_cast<int>(
                (MUSIC_LIST_BOTTOM - MUSIC_LIST_TOP) /
                MUSIC_ROW_HEIGHT
                );

        return calculated > 0 ? calculated : 1;
    }

    int GetMusicMaxScrollOffset()
    {
        const int itemCount =
            static_cast<int>(
                Playlist::GetTracks().size()
                );

        return std::max(
            0,
            itemCount - GetMusicVisibleRows()
        );
    }

    int GetSettingsVisibleRows()
    {
        return 5;
    }

    int GetSettingsMaxScrollOffset()
    {
        constexpr int SETTINGS_ITEM_COUNT = 9;

        return std::max(
            0,
            SETTINGS_ITEM_COUNT -
            GetSettingsVisibleRows()
        );
    }

    void SetMusicScrollOffset(int offset)
    {
        const int maxOffset =
            GetMusicMaxScrollOffset();

        g_musicScrollOffset =
            std::max(
                0,
                std::min(offset, maxOffset)
            );

        if (g_menuSelection < g_musicScrollOffset)
        {
            g_menuSelection =
                g_musicScrollOffset;
        }

        const int visibleRows =
            GetMusicVisibleRows();

        if (g_menuSelection >=
            g_musicScrollOffset + visibleRows)
        {
            g_menuSelection =
                g_musicScrollOffset +
                visibleRows - 1;
        }
    }

    void SetSettingsScrollOffset(int offset)
    {
        const int maxOffset =
            GetSettingsMaxScrollOffset();

        g_settingsScrollOffset =
            std::max(
                0,
                std::min(offset, maxOffset)
            );

        const int visibleRows =
            GetSettingsVisibleRows();

        if (g_menuSelection < g_settingsScrollOffset)
        {
            g_menuSelection =
                g_settingsScrollOffset;
        }

        if (g_menuSelection >=
            g_settingsScrollOffset + visibleRows)
        {
            g_menuSelection =
                g_settingsScrollOffset +
                visibleRows - 1;
        }
    }

    void MoveTabLeft()
    {
        switch (g_currentTab)
        {
        case MenuTab::MUSIC:
            g_currentTab = MenuTab::CREDITS;
            break;

        case MenuTab::SETTINGS:
            g_currentTab = MenuTab::MUSIC;
            break;

        case MenuTab::CREDITS:
            g_currentTab = MenuTab::SETTINGS;
            break;
        }

        g_menuSelection = 0;
        g_musicScrollOffset = 0;
        g_settingsScrollOffset = 0;
        g_musicScrollbarDragging = false;
        g_settingsScrollbarDragging = false;
    }

    void MoveTabRight()
    {
        switch (g_currentTab)
        {
        case MenuTab::MUSIC:
            g_currentTab = MenuTab::SETTINGS;
            break;

        case MenuTab::SETTINGS:
            g_currentTab = MenuTab::CREDITS;
            break;

        case MenuTab::CREDITS:
            g_currentTab = MenuTab::MUSIC;
            break;
        }

        g_menuSelection = 0;
        g_musicScrollOffset = 0;
        g_settingsScrollOffset = 0;
    }

    void MoveContentUp()
    {
        const int itemCount =
            GetCurrentTabItemCount();

        if (itemCount <= 0)
            return;

        if (g_menuSelection > 0)
        {
            g_menuSelection--;

            if (g_currentTab == MenuTab::MUSIC)
            {
                EnsureMusicSelectionVisible();
            }
            else if (g_currentTab == MenuTab::SETTINGS)
            {
                EnsureSettingsSelectionVisible();
            }
        }
    }

    void MoveContentDown()
    {
        const int itemCount =
            GetCurrentTabItemCount();

        if (itemCount <= 0)
            return;

        if (g_menuSelection < itemCount - 1)
        {
            g_menuSelection++;

            if (g_currentTab == MenuTab::MUSIC)
            {
                EnsureMusicSelectionVisible();
            }
            else if (g_currentTab == MenuTab::SETTINGS)
            {
                EnsureSettingsSelectionVisible();
            }
        }
    }

    void PlaySelectedMusic()
    {
        if (g_currentTab != MenuTab::MUSIC)
            return;

        const auto& tracks =
            Playlist::GetTracks();

        if (tracks.empty())
            return;

        if (g_menuSelection < 0 ||
            g_menuSelection >=
            static_cast<int>(tracks.size()))
            return;

        Logger::Info(
            ("MusicPlayerUI: play selected track: " +
                tracks[g_menuSelection].name).c_str()
        );

        MusicManager::PlayTrack(
            static_cast<size_t>(g_menuSelection)
        );
    }

    void ChangeSelectedContext(bool next)
    {
        if (g_currentTab != MenuTab::MUSIC)
            return;

        auto& tracks =
            Playlist::GetTracksMutable();

        if (tracks.empty())
            return;

        if (g_menuSelection < 0 ||
            g_menuSelection >=
            static_cast<int>(tracks.size()))
            return;

        TrackContext oldContext =
            tracks[g_menuSelection].context;

        TrackContext newContext =
            next
            ? GetNextContext(oldContext)
            : GetPreviousContext(oldContext);

        if (!Playlist::SetTrackContext(
            static_cast<size_t>(g_menuSelection),
            newContext))
        {
            Logger::Error(
                "MusicPlayerUI: failed to change track context."
            );
            return;
        }

        if (!Playlist::Save())
        {
            Logger::Error(
                "MusicPlayerUI: failed to save data.dat."
            );
            return;
        }

        MusicManager::RefreshPlaylist();

        Logger::Info(
            "MusicPlayerUI: track context saved."
        );
    }

    void TogglePlayPause()
    {
        if (MusicManager::IsPaused())
        {
            MusicManager::Resume();
            return;
        }

        if (MusicManager::IsPlaying())
        {
            MusicManager::Pause();
            return;
        }

        MusicManager::Play();
    }

    void ToggleShuffle()
    {
        MusicManager::ToggleShuffle();
    }

    void ToggleMute()
    {
        AudioEngine::ToggleMute();
    }

    void HandleMouseMenuClick(const POINT& mouse)
    {
        if (!g_menuOpen)
            return;

        if (PtInRect(&g_closeButton, mouse))
        {
            CloseMenu();
            return;
        }

        if (g_currentTab == MenuTab::MUSIC &&
            PtInRect(&g_musicScrollbarTrack, mouse))
        {
            if (PtInRect(&g_musicScrollbarThumb, mouse))
            {
                g_musicScrollbarDragging = true;

                g_musicScrollbarDragOffset =
                    mouse.y -
                    g_musicScrollbarThumb.top;
            }
            else
            {
                ScrollMusicFromScrollbarY(mouse.y);
            }

            return;
        }

        if (g_currentTab == MenuTab::SETTINGS &&
            PtInRect(&g_settingsScrollbarTrack, mouse))
        {
            if (PtInRect(&g_settingsScrollbarThumb, mouse))
            {
                g_settingsScrollbarDragging = true;

                g_settingsScrollbarDragOffset =
                    mouse.y -
                    g_settingsScrollbarThumb.top;
            }
            else
            {
                ScrollSettingsFromScrollbarY(mouse.y);
            }

            return;
        }

        const LONG tabTop =
            g_mainMenuRect.top + 60;

        const LONG tabHeight = 35;
        const LONG tabWidth = 180;
        const LONG tabGap = 10;

        RECT musicTab =
        {
            g_mainMenuRect.left + 15,
            tabTop,
            g_mainMenuRect.left + 15 + tabWidth,
            tabTop + tabHeight
        };

        RECT settingsTab =
        {
            musicTab.right + tabGap,
            tabTop,
            musicTab.right + tabGap + tabWidth,
            tabTop + tabHeight
        };

        RECT creditsTab =
        {
            settingsTab.right + tabGap,
            tabTop,
            settingsTab.right + tabGap + tabWidth,
            tabTop + tabHeight
        };

        if (PtInRect(&musicTab, mouse))
        {
            g_currentTab = MenuTab::MUSIC;
            g_menuFocus = MenuFocus::TABS;
            g_menuSelection = 0;
            g_musicScrollOffset = 0;
            g_musicScrollbarDragging = false;
            g_settingsScrollbarDragging = false;
            return;
        }

        if (PtInRect(&settingsTab, mouse))
        {
            g_currentTab = MenuTab::SETTINGS;
            g_menuFocus = MenuFocus::TABS;
            g_menuSelection = 0;
            g_settingsScrollOffset = 0;
            g_musicScrollbarDragging = false;
            g_settingsScrollbarDragging = false;
            return;
        }

        if (PtInRect(&creditsTab, mouse))
        {
            g_currentTab = MenuTab::CREDITS;
            g_menuFocus = MenuFocus::TABS;
            g_menuSelection = 0;
            return;
        }

        if (PtInRect(&g_menuPreviousButton, mouse))
        {
            MusicManager::Previous();
            return;
        }

        if (PtInRect(&g_menuPlayPauseButton, mouse))
        {
            TogglePlayPause();
            return;
        }

        if (PtInRect(&g_menuNextButton, mouse))
        {
            MusicManager::Next();
            return;
        }

        if (PtInRect(&g_menuShuffleButton, mouse))
        {
            Logger::Info(
                "MusicPlayerUI: shuffle button clicked."
            );

            MusicManager::ToggleShuffle();
            return;
        }

        if (PtInRect(&g_menuMuteButton, mouse))
        {
            Logger::Info(
                "MusicPlayerUI: mute button clicked."
            );

            AudioEngine::ToggleMute();
            return;
        }

        if (g_currentTab == MenuTab::MUSIC)
        {
            for (size_t row = 0;
                row < g_musicRowRects.size();
                ++row)
            {
                if (!PtInRect(
                    &g_musicRowRects[row],
                    mouse))
                {
                    continue;
                }

                const int trackIndex =
                    g_musicScrollOffset +
                    static_cast<int>(row);

                const auto& tracks =
                    Playlist::GetTracks();

                if (trackIndex < 0 ||
                    trackIndex >=
                    static_cast<int>(tracks.size()))
                    return;

                g_menuFocus = MenuFocus::CONTENT;
                g_menuSelection = trackIndex;

                if (row < g_musicContextRects.size() &&
                    PtInRect(
                        &g_musicContextRects[row],
                        mouse))
                {
                    ChangeSelectedContext(true);
                }
                else
                {
                    PlaySelectedMusic();
                }

                return;
            }
        }

        if (g_currentTab == MenuTab::SETTINGS)
        {
            const LONG contentLeft =
                g_mainMenuRect.left + 40;

            const LONG contentTop =
                g_mainMenuRect.top + 120;

            const LONG contentRight =
                g_mainMenuRect.right - 40;

            const LONG rowHeight = 42;
            const LONG labelWidth = 220;

            const LONG valueLeft =
                contentLeft + labelWidth;

            const int VISIBLE_ROWS =
                GetSettingsVisibleRows();

            for (int visibleRow = 0;
                visibleRow < VISIBLE_ROWS;
                ++visibleRow)
            {
                const int logicalIndex =
                    g_settingsScrollOffset +
                    visibleRow;

                if (logicalIndex >=
                    GetCurrentTabItemCount())
                {
                    break;
                }

                RECT rowRect =
                {
                    contentLeft,
                    contentTop +
                        visibleRow * rowHeight,
                    contentRight,
                    contentTop +
                        (visibleRow + 1) * rowHeight
                };

                if (!PtInRect(&rowRect, mouse))
                    continue;

                g_menuFocus =
                    MenuFocus::CONTENT;

                g_menuSelection =
                    logicalIndex;

                RECT valueRect =
                {
                    valueLeft,
                    rowRect.top,
                    valueLeft + 150,
                    rowRect.bottom
                };

                constexpr LONG ARROW_WIDTH = 20;

                RECT leftArrowRect =
                {
                    valueRect.left,
                    valueRect.top,
                    valueRect.left + ARROW_WIDTH,
                    valueRect.bottom
                };

                RECT rightArrowRect =
                {
                    valueRect.right - ARROW_WIDTH,
                    valueRect.top,
                    valueRect.right,
                    valueRect.bottom
                };

                if (PtInRect(
                    &leftArrowRect,
                    mouse))
                {
                    AdjustSettingsValue(false);
                }
                else if (PtInRect(
                    &rightArrowRect,
                    mouse))
                {
                    AdjustSettingsValue(true);
                }

                return;
            }
        }
    }

    void UpdateMenuMouse()
    {
        POINT mouse = {};

        if (!GetMousePosition(mouse))
            return;

        // Reset semua hover state.
        g_closeHovered = false;
        g_musicTabHovered = false;
        g_settingsTabHovered = false;
        g_creditsTabHovered = false;

        g_musicRowHovered = false;
        g_musicHoveredRow = -1;
        g_musicContextHovered = false;

        g_menuPreviousHovered = false;
        g_menuPlayPauseHovered = false;
        g_menuNextHovered = false;
        g_menuShuffleHovered = false;
        g_menuMuteHovered = false;

        g_menuPreviousHovered =
            PtInRect(&g_menuPreviousButton, mouse) != FALSE;

        g_menuPlayPauseHovered =
            PtInRect(&g_menuPlayPauseButton, mouse) != FALSE;

        g_menuNextHovered =
            PtInRect(&g_menuNextButton, mouse) != FALSE;

        g_menuShuffleHovered =
            PtInRect(&g_menuShuffleButton, mouse) != FALSE;

        g_menuMuteHovered =
            PtInRect(&g_menuMuteButton, mouse) != FALSE;

        // -------------------------------------------------
        // MAIN MENU CLOSE
        // -------------------------------------------------

        if (g_menuOpen &&
            PtInRect(&g_closeButton, mouse))
        {
            g_closeHovered = true;
        }

        // -------------------------------------------------
        // TABS
        // -------------------------------------------------

        if (g_menuOpen)
        {
            RECT musicTab = {};
            RECT settingsTab = {};
            RECT creditsTab = {};

            GetMenuTabRects(
                musicTab,
                settingsTab,
                creditsTab
            );

            g_musicTabHovered =
                PtInRect(&musicTab, mouse) != FALSE;

            g_settingsTabHovered =
                PtInRect(&settingsTab, mouse) != FALSE;

            g_creditsTabHovered =
                PtInRect(&creditsTab, mouse) != FALSE;
        }

        // -------------------------------------------------
        // MUSIC PLAYLIST
        // -------------------------------------------------

        // -------------------------------------------------
        // SCROLLBAR HOVER
        // -------------------------------------------------

        g_musicScrollbarHovered = false;
        g_settingsScrollbarHovered = false;

        if (!g_menuOpen)
            return;

        if (g_currentTab == MenuTab::MUSIC)
        {
            g_musicScrollbarHovered =
                GetMusicMaxScrollOffset() > 0 &&
                PtInRect(
                    &g_musicScrollbarThumb,
                    mouse
                ) != FALSE;
        }
        else if (g_currentTab == MenuTab::SETTINGS)
        {
            g_settingsScrollbarHovered =
                GetSettingsMaxScrollOffset() > 0 &&
                PtInRect(
                    &g_settingsScrollbarThumb,
                    mouse
                ) != FALSE;
        }

        if (g_currentTab == MenuTab::MUSIC)
        {
            for (size_t row = 0;
                row < g_musicRowRects.size();
                ++row)
            {
                if (!PtInRect(
                    &g_musicRowRects[row],
                    mouse))
                {
                    continue;
                }

                const int trackIndex =
                    g_musicScrollOffset +
                    static_cast<int>(row);

                const auto& tracks =
                    Playlist::GetTracks();

                if (trackIndex < 0 ||
                    trackIndex >=
                    static_cast<int>(tracks.size()))
                {
                    return;
                }

                g_musicRowHovered = true;
                g_musicHoveredRow =
                    static_cast<int>(row);

                if (row < g_musicContextRects.size() &&
                    PtInRect(
                        &g_musicContextRects[row],
                        mouse))
                {
                    g_musicContextHovered = true;
                }

                return;
            }
        }
    }

    void AdjustSettingsValue(bool increase)
    {
        if (g_currentTab != MenuTab::SETTINGS)
            return;

        switch (g_menuSelection)
        {
            case 0:
            {
                float volume =
                    Settings::GetMasterVolume();

                volume += increase
                    ? 0.05f
                    : -0.05f;

                if (volume < 0.0f)
                    volume = 0.0f;

                if (volume > 1.0f)
                    volume = 1.0f;

                Settings::SetMasterVolume(volume);
                AudioEngine::SetVolume(volume);
                Settings::Save();

                break;
            }

            case 1:
            {
                const bool enabled =
                    !Settings::IsPausedLPFEnabled();

                Settings::SetPausedLPFEnabled(enabled);

                if (GameStateManager::GetState() == GameState::GAMEPLAY &&
                    GameStateManager::GetGameplaySubState() ==
                    GameplaySubState::PAUSED)
                {
                    AudioEngine::SetLowPassEnabled(enabled);
                }
                else
                {
                    AudioEngine::SetLowPassEnabled(false);
                }

                Settings::Save();

                break;
            }

            case 2:
            {
                const bool enabled =
                    !DynamicAudio::IsSpeedSensitiveEnabled();

                Settings::SetSpeedSensitiveAudioEnabled(enabled);
                DynamicAudio::SetSpeedSensitiveEnabled(enabled);

                Settings::Save();

                break;
            }

            case 3:
            {
                const bool enabled =
                    !DynamicAudio::IsInteriorAudioOnIdleEnabled();

                Settings::SetInteriorAudioOnIdleEnabled(enabled);
                DynamicAudio::SetInteriorAudioOnIdleEnabled(enabled);

                Settings::Save();

                break;
            }

            case 4:
            {
                const bool enabled =
                    !DynamicAudio::IsSpeedbreakerAudioEnabled();

                Settings::SetSpeedbreakerAudioEnabled(enabled);
                DynamicAudio::SetSpeedbreakerAudioEnabled(enabled);

                Settings::Save();

                break;
            }

            case 5:
            {
                const bool enabled =
                    !Settings::IsFEReverbEnabled();

                Settings::SetFEReverbEnabled(enabled);

                if (GameStateManager::GetState() == GameState::FE)
                {
                    AudioEngine::SetFEReverbEnabled(enabled);
                }
                else
                {
                    AudioEngine::SetFEReverbEnabled(false);
                }

                Settings::Save();

                break;
            }

            case 6:
            {
                const bool enabled =
                    !Settings::IsFEEQEnabled();

                Settings::SetFEEQEnabled(enabled);

                if (GameStateManager::GetState() == GameState::FE)
                {
                    AudioEngine::SetFEEQEnabled(enabled);
                }
                else
                {
                    AudioEngine::SetFEEQEnabled(false);
                }

                Settings::Save();

                break;
            }

            case 7:
            {
                float mix =
                    Settings::GetFEEffectMix();

                mix += increase
                    ? 10.0f
                    : -10.0f;

                if (mix < 0.0f)
                    mix = 0.0f;

                if (mix > 100.0f)
                    mix = 100.0f;

                Settings::SetFEEffectMix(mix);
                AudioEngine::SetFEEffectMix(mix);

                Settings::Save();

                break;
            }

            case 8:
            {
                const int newSkin =
                    Settings::GetMiniPlayerSkin() == 1
                    ? 0
                    : 1;

                Settings::SetMiniPlayerSkin(newSkin);

                g_miniPlayerSkin =
                    newSkin == 1
                    ? MiniPlayerSkin::IPOD_CLASSIC_5
                    : MiniPlayerSkin::SIMPLE;

                Settings::Save();

                break;
            }
        }
    }

    void UpdateMenuInput()
    {
        const bool upPressed =
            IsKeyPressed(Settings::GetMenuUpKey());

        const bool downPressed =
            IsKeyPressed(Settings::GetMenuDownKey());

        const bool leftPressed =
            IsKeyPressed(Settings::GetMenuLeftKey());

        const bool rightPressed =
            IsKeyPressed(Settings::GetMenuRightKey());

        const bool backPressed =
            IsKeyPressed(Settings::GetMenuBackKey());

        const bool enterPressed =
            IsKeyPressed(Settings::GetMenuEnterKey());

        const bool mousePressed =
            IsMouseButtonPressed();

        if (backPressed)
        {
            if (g_menuFocus == MenuFocus::TABS)
            {
                CloseMenu();
                return;
            }

            if (g_menuFocus == MenuFocus::CONTENT)
            {
                g_menuFocus = MenuFocus::TABS;
                g_menuSelection = 0;
                return;
            }

            if (g_menuFocus == MenuFocus::PLAYER)
            {
                g_menuFocus = MenuFocus::CONTENT;
                return;
            }
        }

        UpdateMenuMouse();
        UpdateMouseScrollbarDrag();

        if (mousePressed)
        {
            POINT mouse = {};

            if (GetMousePosition(mouse))
            {
                HandleMouseMenuClick(mouse);
                return;
            }
        }

        switch (g_menuFocus)
        {
        case MenuFocus::TABS:
        {
            if (leftPressed)
                MoveTabLeft();

            if (rightPressed)
                MoveTabRight();

            if (downPressed)
            {
                g_menuFocus = MenuFocus::CONTENT;
                g_menuSelection = 0;
                g_musicScrollOffset = 0;
                g_settingsScrollOffset = 0;
            }

            break;
        }

        case MenuFocus::CONTENT:
        {
            if (upPressed)
            {
                MoveContentUp();
            }

            if (downPressed)
            {
                MoveContentDown();
            }

            // -------------------------------------------------
            // MUSIC TAB
            // -------------------------------------------------

            if (g_currentTab == MenuTab::MUSIC)
            {
                if (leftPressed)
                {
                    ChangeSelectedContext(false);
                }

                if (rightPressed)
                {
                    ChangeSelectedContext(true);
                }

                if (enterPressed)
                {
                    PlaySelectedMusic();
                }
            }

            // -------------------------------------------------
            // SETTINGS TAB
            // -------------------------------------------------

            if (g_currentTab == MenuTab::SETTINGS)
            {
                if (leftPressed)
                {
                    AdjustSettingsValue(false);
                }

                if (rightPressed)
                {
                    AdjustSettingsValue(true);
                }
            }

            // -------------------------------------------------
            // CREDITS TAB
            // -------------------------------------------------

            if (g_currentTab == MenuTab::CREDITS)
            {
                if (enterPressed)
                {
                    Logger::Info(
                        "MusicPlayerUI: credits selected."
                    );
                }
            }

            break;
        }
        }
    }
}

void UpdateFancyMiniPlayerHitboxes(
    const RECT& player)
{
    const float sx =
        static_cast<float>(
            player.right - player.left
            ) / 312.0f;

    const float sy =
        static_cast<float>(
            player.bottom - player.top
            ) / 508.0f;

    const LONG X = player.left;
    const LONG Y = player.top;

    // Previous
    g_previousButton =
    {
        X + (LONG)std::lround(65.0f * sx),
        Y + (LONG)std::lround(310.0f * sy),
        X + (LONG)std::lround(108.0f * sx),
        Y + (LONG)std::lround(367.0f * sy)
    };

    // Next
    g_nextButton =
    {
        X + (LONG)std::lround(204.0f * sx),
        Y + (LONG)std::lround(310.0f * sy),
        X + (LONG)std::lround(247.0f * sx),
        Y + (LONG)std::lround(367.0f * sy)
    };

    // Play / pause
    g_playPauseButton =
    {
        X + (LONG)std::lround(125.0f * sx),
        Y + (LONG)std::lround(389.0f * sy),
        X + (LONG)std::lround(187.0f * sx),
        Y + (LONG)std::lround(438.0f * sy)
    };

    // MENU
    g_menuButton =
    {
        X + (LONG)std::lround(112.0f * sx),
        Y + (LONG)std::lround(244.0f * sy),
        X + (LONG)std::lround(200.0f * sx),
        Y + (LONG)std::lround(291.0f * sy)
    };
}

void RenderFancyMiniPlayer(
    LPDIRECT3DDEVICE9 device,
    float dt)
{
    if (!g_fancyMiniPlayerTexture)
        return;

    if (!g_miniFont)
        return;

    const RECT& player =
        g_playerRect;

    // Base iPod texture.
    DrawTexture(
        device,
        g_fancyMiniPlayerTexture,
        player
    );

    const float sx =
        static_cast<float>(
            player.right - player.left
            ) / 312.0f;

    const float sy =
        static_cast<float>(
            player.bottom - player.top
            ) / 508.0f;

    const LONG X = player.left;
    const LONG Y = player.top;

    // "6 of 15" above album art.
    RECT positionRect =
    {
        X + (LONG)std::lround(52.0f * sx),
        Y + (LONG)std::lround(55.0f * sy),
        X + (LONG)std::lround(141.0f * sx),
        Y + (LONG)std::lround(72.0f * sy)
    };

    DrawMiniPosition(
        device,
        positionRect
    );

    const Track* currentTrack =
        MusicManager::GetCurrentTrack();

    if (currentTrack)
    {
        RECT artistRect =
        {
            X + (LONG)std::lround(149.0f * sx),
            Y + (LONG)std::lround(73.0f * sy),
            X + (LONG)std::lround(268.0f * sx),
            Y + (LONG)std::lround(96.0f * sy)
        };

        RECT titleRect =
        {
            X + (LONG)std::lround(149.0f * sx),
            Y + (LONG)std::lround(96.0f * sy),
            X + (LONG)std::lround(268.0f * sx),
            Y + (LONG)std::lround(119.0f * sy)
        };

        RECT albumRect =
        {
            X + (LONG)std::lround(149.0f * sx),
            Y + (LONG)std::lround(119.0f * sy),
            X + (LONG)std::lround(268.0f * sx),
            Y + (LONG)std::lround(142.0f * sy)
        };

        DrawScrollingText(
            device,
            g_miniFont,
            currentTrack->artist.c_str(),
            artistRect,
            g_artistMarquee,
            D3DCOLOR_ARGB(
                255, 60, 70, 75
            ),
            D3DCOLOR_ARGB(
                255, 235, 242, 247
            ),
            dt
        );

        DrawScrollingText(
            device,
            g_miniFont,
            currentTrack->name.c_str(),
            titleRect,
            g_titleMarquee,
            D3DCOLOR_ARGB(
                255, 60, 70, 75
            ),
            D3DCOLOR_ARGB(
                255, 235, 242, 247
            ),
            dt
        );

        DrawScrollingText(
            device,
            g_miniFont,
            currentTrack->album.c_str(),
            albumRect,
            g_albumMarquee,
            D3DCOLOR_ARGB(
                255, 60, 70, 75
            ),
            D3DCOLOR_ARGB(
                255, 235, 242, 247
            ),
            dt
        );
    }
}

void RenderSimpleMiniPlayer(
    LPDIRECT3DDEVICE9 device,
    float dt)
{
    // ---------------------------------------------------------
    // MINI PLAYER BACKGROUND
    // ---------------------------------------------------------

    DrawRect(
        device,
        g_playerRect,
        COLOR_PANEL
    );

    // ---------------------------------------------------------
    // CURRENT TRACK INFO
    // ---------------------------------------------------------

    const Track* currentTrack =
        MusicManager::GetCurrentTrack();

    if (currentTrack)
    {
        RECT titleRect =
        {
            g_playerRect.left + 20,
            g_playerRect.top + 18,
            g_playerRect.right - 20,
            g_playerRect.top + 43
        };

        RECT artistRect =
        {
            g_playerRect.left + 20,
            g_playerRect.top + 43,
            g_playerRect.right - 20,
            g_playerRect.top + 68
        };

        RECT albumRect =
        {
            g_playerRect.left + 20,
            g_playerRect.top + 68,
            g_playerRect.right - 20,
            g_playerRect.top + 93
        };

        DrawScrollingText(
            device,
            g_miniFont,
            currentTrack->name.c_str(),
            titleRect,
            g_titleMarquee,
            COLOR_TEXT,
            COLOR_PANEL,
            dt
        );

        DrawScrollingText(
            device,
            g_miniFont,
            currentTrack->artist.c_str(),
            artistRect,
            g_artistMarquee,
            COLOR_TEXT,
            COLOR_PANEL,
            dt
        );

        DrawScrollingText(
            device,
            g_miniFont,
            currentTrack->album.c_str(),
            albumRect,
            g_albumMarquee,
            COLOR_TEXT,
            COLOR_PANEL,
            dt
        );
    }
    else
    {
        DrawText(
            device,
            "No music",
            {
                g_playerRect.left + 10,
                g_playerRect.top + 45,
                g_playerRect.right - 10,
                g_playerRect.top + 75
            },
            DT_LEFT | DT_SINGLELINE
        );
    }

    // ---------------------------------------------------------
    // PLAYER BUTTONS
    // ---------------------------------------------------------

    const RECT previousVisual =
        GetCenteredTextureRect(
            g_previousButton,
            38
        );

    const RECT playPauseVisual =
        GetCenteredTextureRect(
            g_playPauseButton,
            46
        );

    const RECT nextVisual =
        GetCenteredTextureRect(
            g_nextButton,
            38
        );

    // Previous
    DrawPlayerTextureButton(
        device,
        previousVisual,
        g_previousTexture,
        g_previousHoverTexture,
        g_previousHovered
    );

    // Play / Pause
    if (MusicManager::IsPaused())
    {
        DrawPlayerTextureButton(
            device,
            playPauseVisual,
            g_playTexture,
            g_playHoverTexture,
            g_playPauseHovered
        );
    }
    else
    {
        DrawPlayerTextureButton(
            device,
            playPauseVisual,
            g_pauseTexture,
            g_pauseHoverTexture,
            g_playPauseHovered
        );
    }

    // Next
    DrawPlayerTextureButton(
        device,
        nextVisual,
        g_nextTexture,
        g_nextHoverTexture,
        g_nextHovered
    );
}

namespace MusicPlayerUI
{
    bool Init()
    {
        g_initialized = true;
        g_enabled = false;

        g_menuOpen = false;

        g_miniPlayerSkin =
            Settings::GetMiniPlayerSkin() == 1
            ? MiniPlayerSkin::IPOD_CLASSIC_5
            : MiniPlayerSkin::SIMPLE;

        g_miniWantedVisible = false;
        g_miniVisibilityInitialized = false;
        g_miniAnimationActive = false;

        g_miniLastScreenWidth = 0;
        g_miniLastScreenHeight = 0;

        g_artistMarquee = {};
        g_titleMarquee = {};
        g_albumMarquee = {};

        Logger::Info(
            "MusicPlayerUI initialized."
        );

        return true;
    }

    void Shutdown()
    {
        if (g_font)
        {
            g_font->Release();
            g_font = nullptr;
            g_fontHeight = 0;
        }

        if (g_miniFont)
        {
            g_miniFont->Release();
            g_miniFont = nullptr;
            g_miniFontHeight = 0;
        }

        if (g_fancyMiniPlayerTexture)
        {
            g_fancyMiniPlayerTexture->Release();
            g_fancyMiniPlayerTexture = nullptr;
        }

        if (g_previousTexture)
        {
            g_previousTexture->Release();
            g_previousTexture = nullptr;
        }

        if (g_previousHoverTexture)
        {
            g_previousHoverTexture->Release();
            g_previousHoverTexture = nullptr;
        }

        if (g_playTexture)
        {
            g_playTexture->Release();
            g_playTexture = nullptr;
        }

        if (g_playHoverTexture)
        {
            g_playHoverTexture->Release();
            g_playHoverTexture = nullptr;
        }

        if (g_pauseTexture)
        {
            g_pauseTexture->Release();
            g_pauseTexture = nullptr;
        }

        if (g_pauseHoverTexture)
        {
            g_pauseHoverTexture->Release();
            g_pauseHoverTexture = nullptr;
        }

        if (g_nextTexture)
        {
            g_nextTexture->Release();
            g_nextTexture = nullptr;
        }

        if (g_nextHoverTexture)
        {
            g_nextHoverTexture->Release();
            g_nextHoverTexture = nullptr;
        }

        if (g_shuffleTexture)
        {
            g_shuffleTexture->Release();
            g_shuffleTexture = nullptr;
        }

        if (g_shuffleHoverTexture)
        {
            g_shuffleHoverTexture->Release();
            g_shuffleHoverTexture = nullptr;
        }

        if (g_shuffleOnTexture)
        {
            g_shuffleOnTexture->Release();
            g_shuffleOnTexture = nullptr;
        }

        if (g_muteTexture)
        {
            g_muteTexture->Release();
            g_muteTexture = nullptr;
        }

        if (g_muteHoverTexture)
        {
            g_muteHoverTexture->Release();
            g_muteHoverTexture = nullptr;
        }

        if (g_muteOnTexture)
        {
            g_muteOnTexture->Release();
            g_muteOnTexture = nullptr;
        }

        if (g_cursorTexture)
        {
            g_cursorTexture->Release();
            g_cursorTexture = nullptr;
        }

        if (g_uiStateBlock)
        {
            g_uiStateBlock->Release();
            g_uiStateBlock = nullptr;
        }

        g_musicRowRects.clear();
        g_musicContextRects.clear();

        g_initialized = false;

        Logger::Info(
            "MusicPlayerUI shutdown."
        );
    }

    bool IsEnabled()
    {
        return g_enabled;
    }

    void SetEnabled(bool enabled)
    {
        g_enabled = enabled;

        SetMiniPlayerTargetVisible(
            g_enabled && !g_menuOpen
        );

        Logger::Info(
            enabled
            ? "MusicPlayerUI enabled."
            : "MusicPlayerUI disabled."
        );
    }

    void OnLostDevice()
    {
        if (g_font)
            g_font->OnLostDevice();

        if (g_miniFont)
            g_miniFont->OnLostDevice();

        if (g_uiStateBlock)
        {
            g_uiStateBlock->Release();
            g_uiStateBlock = nullptr;
        }
    }

    void OnResetDevice()
    {
        if (g_font)
            g_font->OnResetDevice();

        if (g_miniFont)
            g_miniFont->OnResetDevice();
    }

    void Update()
    {
        if (!g_initialized)
            return;

        const float dt =
            GetSafeGameDeltaTime();

        UpdateMiniPlayerAnimation(dt);

        if (IsKeyPressed(Settings::GetUIToggleKey()))
        {
            SetEnabled(!g_enabled);
        }

        const bool previousPressed =
            IsKeyPressed(Settings::GetPlayerPreviousKey());

        const bool playPausePressed =
            IsKeyPressed(Settings::GetPlayerPlayPauseKey());

        const bool nextPressed =
            IsKeyPressed(Settings::GetPlayerNextKey());

        const bool shufflePressed =
            IsKeyPressed(Settings::GetPlayerShuffleKey());

        const bool mutePressed =
            IsKeyPressed(Settings::GetPlayerMuteKey());

        const bool menuPressed =
            IsKeyPressed(Settings::GetToggleMenuKey());

        if (menuPressed)
        {
            ToggleMenu();
        }

        if (!g_enabled)
            return;

        if (g_menuOpen)
        {
            if (previousPressed)
            {
                Logger::Info(
                    "MusicPlayerUI: previous."
                );

                MusicManager::Previous();
            }

            if (playPausePressed)
            {
                Logger::Info(
                    "MusicPlayerUI: play/pause."
                );

                TogglePlayPause();
            }

            if (nextPressed)
            {
                Logger::Info(
                    "MusicPlayerUI: next."
                );

                MusicManager::Next();
            }

            if (shufflePressed)
            {
                Logger::Info(
                    "MusicPlayerUI: shuffle."
                );

                ToggleShuffle();
            }

            if (mutePressed)
            {
                Logger::Info(
                    "MusicPlayerUI: mute."
                );

                ToggleMute();
            }

            UpdateMenuInput();
            return;
        }

        const bool mousePressed =
            IsMouseButtonPressed();

        if (previousPressed)
        {
            Logger::Info(
                "MusicPlayerUI: previous."
            );

            MusicManager::Previous();
        }

        if (playPausePressed)
        {
            Logger::Info(
                "MusicPlayerUI: play/pause."
            );

            TogglePlayPause();
        }

        if (nextPressed)
        {
            Logger::Info(
                "MusicPlayerUI: next."
            );

            MusicManager::Next();
        }

        POINT mouse = {};

        if (!GetMousePosition(mouse))
        {
            g_previousHovered = false;
            g_playPauseHovered = false;
            g_nextHovered = false;
            g_menuHovered = false;
            g_closeHovered = false;
            return;
        }

        g_previousHovered =
            PtInRect(
                &g_previousButton,
                mouse
            ) != FALSE;

        g_playPauseHovered =
            PtInRect(
                &g_playPauseButton,
                mouse
            ) != FALSE;

        g_nextHovered =
            PtInRect(
                &g_nextButton,
                mouse
            ) != FALSE;

        g_menuHovered =
            PtInRect(
                &g_menuButton,
                mouse
            ) != FALSE;

        if (!mousePressed)
            return;

        if (g_menuHovered)
        {
            ToggleMenu();
        }
        else if (g_previousHovered)
        {
            MusicManager::Previous();
        }
        else if (g_playPauseHovered)
        {
            TogglePlayPause();
        }
        else if (g_nextHovered)
        {
            MusicManager::Next();
        }
    }

    void Render(LPDIRECT3DDEVICE9 device)
    {
        if (!g_initialized)
            return;

        UpdateLayout(device);

        if (!g_enabled &&
            !g_miniAnimationActive)
        {
            return;
        }

        CreateFont(device);

        CreateMiniFont(device);

        if (g_miniPlayerSkin ==
            MiniPlayerSkin::IPOD_CLASSIC_5)
        {
            LoadFancyMiniPlayerTexture(device);
        }

        LoadPlayerTextures(device);

        if (g_menuOpen)
        {

            if (!g_font)
                return;
        }

        if (g_menuOpen)
        {
            if (!g_font)
                return;
        }

        // -------------------------------------------------
        // SAVE GAME'S CURRENT D3D STATE
        // -------------------------------------------------

        if (!g_uiStateBlock)
        {
            if (FAILED(
                device->CreateStateBlock(
                    D3DSBT_ALL,
                    &g_uiStateBlock
                )))
            {
                g_uiStateBlock = nullptr;
            }
        }

        if (g_uiStateBlock)
        {
            g_uiStateBlock->Capture();
        }

        // -------------------------------------------------
        // SET UI RENDER STATE
        // -------------------------------------------------

        device->SetRenderState(
            D3DRS_ALPHABLENDENABLE,
            TRUE
        );

        device->SetRenderState(
            D3DRS_SRCBLEND,
            D3DBLEND_SRCALPHA
        );

        device->SetRenderState(
            D3DRS_DESTBLEND,
            D3DBLEND_INVSRCALPHA
        );

        // -------------------------------------------------
        // RENDER MINI PLAYER
        // -------------------------------------------------

        if (!g_menuOpen || g_miniAnimationActive)
        {
            if (g_miniPlayerSkin ==
                MiniPlayerSkin::IPOD_CLASSIC_5)
            {
                RenderFancyMiniPlayer(
                    device,
                    GetSafeGameDeltaTime()
                );
            }
            else
            {
                RenderSimpleMiniPlayer(
                    device,
                    GetSafeGameDeltaTime()
                );
            }
        }
        else
        {
            if (!g_font)
                return;
        }

        if (g_menuOpen)
        {
            DrawRect(
                device,
                g_mainMenuRect,
                COLOR_PANEL
            );

            RECT titleRect =
            {
                g_mainMenuRect.left + 15,
                g_mainMenuRect.top + 10,
                g_mainMenuRect.right - 60,
                g_mainMenuRect.top + 40
            };

            DrawText(
                device,
                "MENU",
                titleRect,
                DT_LEFT | DT_SINGLELINE
            );

            DrawButton(
                device,
                g_closeButton,
                "X",
                g_closeHovered
            );

            const LONG tabTop =
                g_mainMenuRect.top + 60;

            const LONG tabHeight = 35;
            const LONG tabWidth = 180;
            const LONG tabGap = 10;

            RECT musicTab =
            {
                g_mainMenuRect.left + 15,
                tabTop,
                g_mainMenuRect.left + 15 + tabWidth,
                tabTop + tabHeight
            };

            RECT settingsTab =
            {
                musicTab.right + tabGap,
                tabTop,
                musicTab.right + tabGap + tabWidth,
                tabTop + tabHeight
            };

            RECT creditsTab =
            {
                settingsTab.right + tabGap,
                tabTop,
                settingsTab.right + tabGap + tabWidth,
                tabTop + tabHeight
            };

            DrawButton(
                device,
                musicTab,
                "MUSIC",
                g_musicTabHovered,
                g_currentTab == MenuTab::MUSIC
            );

            DrawButton(
                device,
                settingsTab,
                "SETTINGS",
                g_settingsTabHovered,
                g_currentTab == MenuTab::SETTINGS
            );

            DrawButton(
                device,
                creditsTab,
                "CREDITS",
                g_creditsTabHovered,
                g_currentTab == MenuTab::CREDITS
            );

            if (g_currentTab == MenuTab::MUSIC)
            {
                RECT headerRect =
                {
                    g_mainMenuRect.left + 25,
                    g_mainMenuRect.top + 118,
                    g_mainMenuRect.right - 25,
                    g_mainMenuRect.top + 150
                };

                DrawText(
                    device,
                    "#",
                    {
                        headerRect.left,
                        headerRect.top,
                        headerRect.left + 35,
                        headerRect.bottom
                    },
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                    COLOR_MUTED
                );

                DrawText(
                    device,
                    "TRACK",
                    {
                        headerRect.left + 35,
                        headerRect.top,
                        headerRect.left + 250,
                        headerRect.bottom
                    },
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                    COLOR_MUTED
                );

                DrawText(
                    device,
                    "ARTIST",
                    {
                        headerRect.left + 250,
                        headerRect.top,
                        headerRect.left + 405,
                        headerRect.bottom
                    },
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                    COLOR_MUTED
                );

                DrawText(
                    device,
                    "ALBUM",
                    {
                        headerRect.left + 405,
                        headerRect.top,
                        headerRect.right - 100,
                        headerRect.bottom
                    },
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                    COLOR_MUTED
                );

                DrawText(
                    device,
                    "CONTEXT",
                    {
                        headerRect.right - 95,
                        headerRect.top,
                        headerRect.right,
                        headerRect.bottom
                    },
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                    COLOR_MUTED
                );

                const auto& tracks =
                    Playlist::GetTracks();

                const int visibleRows =
                    static_cast<int>(
                        g_musicRowRects.size()
                        );

                for (int row = 0;
                    row < visibleRows;
                    ++row)
                {
                    const int trackIndex =
                        g_musicScrollOffset + row;

                    if (trackIndex < 0 ||
                        trackIndex >=
                        static_cast<int>(
                            tracks.size()))
                    {
                        break;
                    }

                    const Track& track =
                        tracks[trackIndex];

                    const bool selected =
                        g_menuFocus == MenuFocus::CONTENT &&
                        g_menuSelection == trackIndex;

                    const RECT& rowRect =
                        g_musicRowRects[row];

                    if (selected)
                    {
                        DrawRect(
                            device,
                            rowRect,
                            COLOR_SELECTED
                        );
                    }

                    char numberText[32];
                    sprintf_s(
                        numberText,
                        "%d",
                        trackIndex + 1
                    );

                    DrawText(
                        device,
                        numberText,
                        {
                            rowRect.left,
                            rowRect.top,
                            rowRect.left + 35,
                            rowRect.bottom
                        },
                        DT_LEFT |
                        DT_VCENTER |
                        DT_SINGLELINE
                    );

                    if (selected)
                    {
                        RECT trackRect =
                        {
                            rowRect.left + 35,
                            rowRect.top,
                            rowRect.left + 250,
                            rowRect.bottom
                        };

                        RECT artistRect =
                        {
                            rowRect.left + 250,
                            rowRect.top,
                            rowRect.left + 405,
                            rowRect.bottom
                        };

                        RECT albumRect =
                        {
                            rowRect.left + 405,
                            rowRect.top,
                            rowRect.right - 100,
                            rowRect.bottom
                        };

                        DrawScrollingText(
                            device,
                            g_font,
                            track.name.c_str(),
                            trackRect,
                            g_selectedTitleMarquee,
                            COLOR_TEXT,
                            COLOR_SELECTED,
                            GetSafeGameDeltaTime()
                        );

                        DrawScrollingText(
                            device,
                            g_font,
                            track.artist.c_str(),
                            artistRect,
                            g_selectedArtistMarquee,
                            COLOR_TEXT,
                            COLOR_SELECTED,
                            GetSafeGameDeltaTime()
                        );

                        DrawScrollingText(
                            device,
                            g_font,
                            track.album.c_str(),
                            albumRect,
                            g_selectedAlbumMarquee,
                            COLOR_TEXT,
                            COLOR_SELECTED,
                            GetSafeGameDeltaTime()
                        );
                    }
                    else
                    {
                        DrawText(
                            device,
                            track.name.c_str(),
                            {
                                rowRect.left + 35,
                                rowRect.top,
                                rowRect.left + 250,
                                rowRect.bottom
                            },
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawText(
                            device,
                            track.artist.c_str(),
                            {
                                rowRect.left + 250,
                                rowRect.top,
                                rowRect.left + 405,
                                rowRect.bottom
                            },
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawText(
                            device,
                            track.album.c_str(),
                            {
                                rowRect.left + 405,
                                rowRect.top,
                                rowRect.right - 100,
                                rowRect.bottom
                            },
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );
                    }

                    const RECT& contextRect =
                        g_musicContextRects[row];

                    DrawButton(
                        device,
                        contextRect,
                        GetContextName(track.context),
                        false,
                        selected
                    );
                }

                RECT hintRect =
                {
                    g_mainMenuRect.left + 25,
                    g_mainMenuRect.bottom - 112,
                    g_mainMenuRect.right - 25,
                    g_mainMenuRect.bottom - 88
                };

                if (GetMusicMaxScrollOffset() > 0)
                {
                    DrawScrollbar(
                        device,
                        g_musicScrollbarTrack,
                        g_musicScrollbarThumb,
                        true,
                        g_musicScrollbarHovered ||
                        g_musicScrollbarDragging
                    );
                }

                const std::string musicHint =
                    GetKeyDisplayName(Settings::GetMenuUpKey()) +
                    "/" +
                    GetKeyDisplayName(Settings::GetMenuDownKey()) +
                    " Select   " +
                    GetKeyDisplayName(Settings::GetMenuLeftKey()) +
                    "/" +
                    GetKeyDisplayName(Settings::GetMenuRightKey()) +
                    " Context   " +
                    GetKeyDisplayName(Settings::GetMenuEnterKey()) +
                    " Play   " +
                    GetKeyDisplayName(Settings::GetMenuBackKey()) +
                    " Back";

                DrawText(
                    device,
                    musicHint.c_str(),
                    hintRect,
                    DT_CENTER | DT_SINGLELINE,
                    COLOR_MUTED
                );
            }
            else if (g_currentTab == MenuTab::SETTINGS)
            {
                const LONG contentLeft =
                    g_mainMenuRect.left + 40;

                const LONG contentTop =
                    g_mainMenuRect.top + 120;

                const LONG contentRight =
                    g_mainMenuRect.right - 40;

                const LONG rowHeight = 42;
                const LONG labelWidth = 220;

                const LONG valueLeft =
                    contentLeft + labelWidth;

                const int VISIBLE_ROWS =
                    GetSettingsVisibleRows();

                for (int visibleRow = 0;
                    visibleRow < VISIBLE_ROWS;
                    ++visibleRow)
                {
                    const int logicalIndex =
                        g_settingsScrollOffset + visibleRow;

                    if (logicalIndex >=
                        GetCurrentTabItemCount())
                    {
                        break;
                    }

                    const LONG rowTop =
                        contentTop + visibleRow * rowHeight;

                    RECT rowRect =
                    {
                        contentLeft,
                        rowTop,
                        contentRight,
                        rowTop + rowHeight
                    };

                    if (g_menuFocus == MenuFocus::CONTENT &&
                        g_menuSelection == logicalIndex)
                    {
                        DrawRect(
                            device,
                            rowRect,
                            COLOR_SELECTED
                        );
                    }

                    RECT labelRect =
                    {
                        rowRect.left + 10,
                        rowRect.top,
                        rowRect.left + labelWidth,
                        rowRect.bottom
                    };

                    RECT valueRect =
                    {
                        valueLeft,
                        rowRect.top,
                        valueLeft + 150,
                        rowRect.bottom
                    };

                    const bool selected =
                        g_menuFocus == MenuFocus::CONTENT &&
                        g_menuSelection == logicalIndex;

                    switch (logicalIndex)
                    {
                    case 0:
                    {
                        DrawText(
                            device,
                            "Master Volume",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        char text[32];

                        sprintf_s(
                            text,
                            "%.0f%%",
                            Settings::GetMasterVolume() * 100.0f
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            text,
                            selected
                        );

                        break;
                    }

                    case 1:
                    {
                        DrawText(
                            device,
                            "Paused LPF",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            Settings::IsPausedLPFEnabled()
                            ? "ON"
                            : "OFF",
                            selected
                        );

                        break;
                    }

                    case 2:
                    {
                        DrawText(
                            device,
                            "Speed-Sensitive Audio",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            DynamicAudio::IsSpeedSensitiveEnabled()
                            ? "ON"
                            : "OFF",
                            selected
                        );

                        break;
                    }

                    case 3:
                    {
                        DrawText(
                            device,
                            "Interior Audio On Idle",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            DynamicAudio::IsInteriorAudioOnIdleEnabled()
                            ? "ON"
                            : "OFF",
                            selected
                        );

                        break;
                    }

                    case 4:
                    {
                        DrawText(
                            device,
                            "Speedbreaker LPF",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            DynamicAudio::IsSpeedbreakerAudioEnabled()
                            ? "ON"
                            : "OFF",
                            selected
                        );

                        break;
                    }

                    case 5:
                    {
                        DrawText(
                            device,
                            "FE Reverb",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            Settings::IsFEReverbEnabled()
                            ? "ON"
                            : "OFF",
                            selected
                        );

                        break;
                    }

                    case 6:
                    {
                        DrawText(
                            device,
                            "FE EQ",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            Settings::IsFEEQEnabled()
                            ? "ON"
                            : "OFF",
                            selected
                        );

                        break;
                    }

                    case 7:
                    {
                        DrawText(
                            device,
                            "FE Reverb Mix",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        char text[32];

                        sprintf_s(
                            text,
                            "%.0f%%",
                            Settings::GetFEEffectMix()
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            text,
                            selected
                        );

                        break;
                    }

                    case 8:
                    {
                        DrawText(
                            device,
                            "Mini Player",
                            labelRect,
                            DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE
                        );

                        DrawOptionValue(
                            device,
                            valueRect,
                            Settings::GetMiniPlayerSkin() == 1
                            ? "FANCY"
                            : "SIMPLE",
                            selected
                        );

                        break;
                    }

                    default:
                        break;
                    }
                }

                RECT hintRect =
                {
                    g_mainMenuRect.left + 25,
                    g_mainMenuRect.bottom - 112,
                    g_mainMenuRect.right - 25,
                    g_mainMenuRect.bottom - 88
                };

                if (GetSettingsMaxScrollOffset() > 0)
                {
                    DrawScrollbar(
                        device,
                        g_settingsScrollbarTrack,
                        g_settingsScrollbarThumb,
                        true,
                        g_settingsScrollbarHovered ||
                        g_settingsScrollbarDragging
                    );
                }

                const std::string settingsHint =
                    GetKeyDisplayName(Settings::GetMenuUpKey()) +
                    "/" +
                    GetKeyDisplayName(Settings::GetMenuDownKey()) +
                    " Select   " +
                    GetKeyDisplayName(Settings::GetMenuLeftKey()) +
                    "/" +
                    GetKeyDisplayName(Settings::GetMenuRightKey()) +
                    " Adjust   " +
                    GetKeyDisplayName(Settings::GetMenuBackKey()) +
                    " Back";

                DrawText(
                    device,
                    settingsHint.c_str(),
                    hintRect,
                    DT_CENTER |
                    DT_SINGLELINE,
                    COLOR_MUTED
                );
            }
            else
            {
                RECT contentRect =
                {
                    g_mainMenuRect.left + 25,
                    g_mainMenuRect.top + 120,
                    g_mainMenuRect.right - 25,
                    g_mainMenuRect.bottom - 100
                };

                const LONG lineHeight = 24;
                const LONG titleMarginBottom = 18;
                const LONG creditSpacing = 6;

                RECT titleRect =
                {
                    contentRect.left,
                    contentRect.top,
                    contentRect.right,
                    contentRect.top + lineHeight
                };

                DrawText(
                    device,
                    "Credits",
                    titleRect,
                    DT_LEFT |
                    DT_TOP |
                    DT_SINGLELINE,
                    COLOR_TEXT
                );

                const char* credits[] =
                {
                    "TsudaKageyu - MinHook",
                    "Un4seen Developments Ltd. - BASS Audio Library + BASS_FX",
                    "ThirteenAG - Ultimate ASI Loader",
                    "berkayylmao - NFSPluginSDK (references)",
                    "Zolika1351 - NFSC-SDK (references)",
                    "ARCHIE - NFSC_CustomHUD (references)"
                };

                const LONG creditStartY =
                    contentRect.top +
                    lineHeight +
                    titleMarginBottom;

                for (int i = 0; i < 5; ++i)
                {
                    RECT lineRect =
                    {
                        contentRect.left,
                        creditStartY +
                            i * (lineHeight + creditSpacing),
                        contentRect.right,
                        creditStartY +
                            i * (lineHeight + creditSpacing) +
                            lineHeight
                    };

                    DrawText(
                        device,
                        credits[i],
                        lineRect,
                        DT_LEFT |
                        DT_TOP |
                        DT_SINGLELINE,
                        COLOR_MUTED
                    );
                }
            }

            RECT playerRect =
            {
                g_mainMenuRect.left + 15,
                g_mainMenuRect.bottom - 85,
                g_mainMenuRect.right - 15,
                g_mainMenuRect.bottom - 15
            };

            DrawRect(
                device,
                playerRect,
                COLOR_BUTTON
            );

            const Track* currentTrack =
                MusicManager::GetCurrentTrack();

            if (currentTrack)
            {
                RECT titleRect =
                {
                    playerRect.left + 10,
                    playerRect.top + 8,
                    playerRect.right - 10,
                    playerRect.top + 30
                };

                RECT artistRect =
                {
                    playerRect.left + 10,
                    playerRect.top + 30,
                    playerRect.right - 10,
                    playerRect.top + 52
                };

                DrawScrollingText(
                    device,
                    g_font,
                    currentTrack->name.c_str(),
                    titleRect,
                    g_menuTitleMarquee,
                    COLOR_TEXT,
                    COLOR_BUTTON,
                    GetSafeGameDeltaTime()
                );

                DrawScrollingText(
                    device,
                    g_font,
                    currentTrack->artist.c_str(),
                    artistRect,
                    g_menuArtistMarquee,
                    COLOR_TEXT,
                    COLOR_BUTTON,
                    GetSafeGameDeltaTime()
                );
            }
            else
            {
                DrawText(
                    device,
                    "No music",
                    {
                        playerRect.left + 10,
                        playerRect.top + 15,
                        playerRect.right - 10,
                        playerRect.top + 40
                    },
                    DT_LEFT | DT_SINGLELINE
                );
            }

            const LONG playerButtonSize = 40;
            const LONG playPauseButtonSize = 46;
            const LONG playerButtonGap = 12;

            const LONG playerButtonsWidth =
                playerButtonSize +
                playerButtonGap +
                playPauseButtonSize +
                playerButtonGap +
                playerButtonSize +
                playerButtonGap +
                playerButtonSize +
                playerButtonGap +
                playerButtonSize;

            const LONG playerButtonStart =
                playerRect.left +
                (playerRect.right - playerRect.left -
                    playerButtonsWidth) / 2;

            const LONG playerButtonY =
                playerRect.bottom - 50;

            const LONG playPauseY =
                playerButtonY -
                (playPauseButtonSize - playerButtonSize) / 2;

            g_menuPreviousButton =
            {
                playerButtonStart,
                playerButtonY,
                playerButtonStart + playerButtonSize,
                playerButtonY + playerButtonSize
            };

            g_menuPlayPauseButton =
            {
                g_menuPreviousButton.right + playerButtonGap,
                playPauseY,
                g_menuPreviousButton.right +
                    playerButtonGap +
                    playPauseButtonSize,
                playPauseY + playPauseButtonSize
            };

            g_menuNextButton =
            {
                g_menuPlayPauseButton.right + playerButtonGap,
                playerButtonY,
                g_menuPlayPauseButton.right +
                    playerButtonGap +
                    playerButtonSize,
                playerButtonY + playerButtonSize
            };

            g_menuShuffleButton =
            {
                g_menuNextButton.right + playerButtonGap,
                playerButtonY,
                g_menuNextButton.right +
                    playerButtonGap +
                    playerButtonSize,
                playerButtonY + playerButtonSize
            };

            g_menuMuteButton =
            {
                g_menuShuffleButton.right + playerButtonGap,
                playerButtonY,
                g_menuShuffleButton.right +
                    playerButtonGap +
                    playerButtonSize,
                playerButtonY + playerButtonSize
            };

            const RECT menuPreviousVisual =
                GetCenteredTextureRect(
                    g_menuPreviousButton,
                    playerButtonSize
                );

            const RECT menuPlayPauseVisual =
                GetCenteredTextureRect(
                    g_menuPlayPauseButton,
                    playPauseButtonSize
                );

            const RECT menuNextVisual =
                GetCenteredTextureRect(
                    g_menuNextButton,
                    playerButtonSize
                );

            const RECT menuShuffleVisual =
                GetCenteredTextureRect(
                    g_menuShuffleButton,
                    playerButtonSize
                );

            const RECT menuMuteVisual =
                GetCenteredTextureRect(
                    g_menuMuteButton,
                    playerButtonSize
                );

            // Previous
            DrawPlayerTextureButton(
                device,
                menuPreviousVisual,
                g_previousTexture,
                g_previousHoverTexture,
                g_menuPreviousHovered
            );

            // Play / Pause
            if (MusicManager::IsPaused())
            {
                DrawPlayerTextureButton(
                    device,
                    menuPlayPauseVisual,
                    g_playTexture,
                    g_playHoverTexture,
                    g_menuPlayPauseHovered
                );
            }
            else
            {
                DrawPlayerTextureButton(
                    device,
                    menuPlayPauseVisual,
                    g_pauseTexture,
                    g_pauseHoverTexture,
                    g_menuPlayPauseHovered
                );
            }

            // Next
            DrawPlayerTextureButton(
                device,
                menuNextVisual,
                g_nextTexture,
                g_nextHoverTexture,
                g_menuNextHovered
            );

            // Shuffle
            DrawTogglePlayerTextureButton(
                device,
                menuShuffleVisual,
                g_shuffleTexture,
                g_shuffleHoverTexture,
                g_shuffleOnTexture,
                g_menuShuffleHovered,
                MusicManager::IsShuffleEnabled()
            );

            // Mute
            DrawTogglePlayerTextureButton(
                device,
                menuMuteVisual,
                g_muteTexture,
                g_muteHoverTexture,
                g_muteOnTexture,
                g_menuMuteHovered,
                AudioEngine::IsMuted()
            );
        }
        else
        {
            if (g_miniPlayerSkin ==
                MiniPlayerSkin::IPOD_CLASSIC_5)
            {
                RenderFancyMiniPlayer(
                    device,
                    GetSafeGameDeltaTime()
                );
            }
            else
            {
                RenderSimpleMiniPlayer(
                    device,
                    GetSafeGameDeltaTime()
                );
            }
        }

        RenderCursor(device);
        
        if (g_uiStateBlock)
        {
            g_uiStateBlock->Apply();
        }
    }
}