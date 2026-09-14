#pragma once

#include <d3d9.h>

namespace MusicPlayerUI
{
    bool Init();
    void Shutdown();

    void Update();
    void Render(LPDIRECT3DDEVICE9 device);

    void OnLostDevice();
    void OnResetDevice();

    bool IsEnabled();
    void SetEnabled(bool enabled);
}