#include "pch.h"

#include "game_memory.h"
#include <cstdint>

namespace
{
    constexpr uintptr_t WINDOW_HAS_LOST_FOCUS_ADDRESS = 0xAB0B3C;
    constexpr uintptr_t GAME_STATE_ADDRESS = 0xA99BBC;
    constexpr uintptr_t IS_IN_NIS_ADDRESS = 0xB42EBC;
    constexpr uintptr_t IS_GAMEPLAY_PAUSED_ADDRESS = 0xA8AD18;
    constexpr uintptr_t MENU_STATE_ADDRESS = 0xA9809C;
}

namespace GameMemory
{
    bool HasWindowLostFocus()
    {
        return *reinterpret_cast<bool*>(
            WINDOW_HAS_LOST_FOCUS_ADDRESS
            );
    }

    int GetGameState()
    {
        return *reinterpret_cast<int*>(GAME_STATE_ADDRESS);
    }

    bool IsInNIS()
    {
        return *reinterpret_cast<bool*>(IS_IN_NIS_ADDRESS);
    }

    bool IsGameplayPaused()
    {
        return *reinterpret_cast<bool*>(IS_GAMEPLAY_PAUSED_ADDRESS);
    }

    unsigned char GetMenuState()
    {
        return *reinterpret_cast<unsigned char*>(MENU_STATE_ADDRESS);
    }
}