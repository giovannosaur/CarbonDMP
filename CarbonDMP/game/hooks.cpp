#include "pch.h"

#include "hooks.h"

#include "../src/logger.h"

#include <MinHook.h>

namespace GameHooks
{
    bool Init()
    {
        MH_STATUS status = MH_Initialize();

        if (status != MH_OK)
        {
            Logger::Error("MinHook initialization failed.");
            return false;
        }

        Logger::Info("MinHook initialized successfully.");

        return true;
    }

    void Shutdown()
    {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        Logger::Info("MinHook shutdown.");
    }
}