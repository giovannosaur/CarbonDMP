#pragma once

#include <d3d9.h>

namespace DebugOverlay
{
    bool Init();
    void Shutdown();

    void Render(LPDIRECT3DDEVICE9 device);

    void OnLostDevice();
    void OnResetDevice();
}
