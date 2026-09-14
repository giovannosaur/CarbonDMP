#pragma once

namespace GameMemory
{
    bool HasWindowLostFocus();
    int GetGameState();
    bool IsInNIS();
    bool IsGameplayPaused();
    unsigned char GetMenuState();
}