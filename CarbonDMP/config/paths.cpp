#include "pch.h"

#include "paths.h"

#include <windows.h>

namespace
{
    std::string g_modRoot;
    std::string g_dataFile;
    std::string g_musicDirectory;
}

namespace Paths
{
    bool Init()
    {
        char modulePath[MAX_PATH] = {};

        HMODULE module = GetModuleHandleA("CarbonDMP.asi");

        if (!module)
            return false;

        DWORD length = GetModuleFileNameA(
            module,
            modulePath,
            MAX_PATH
        );

        if (length == 0 || length >= MAX_PATH)
            return false;

        std::string fullPath(modulePath);

        const size_t lastSlash = fullPath.find_last_of("\\/");

        if (lastSlash == std::string::npos)
            return false;

        const std::string asiDirectory =
            fullPath.substr(0, lastSlash);

        g_modRoot = asiDirectory + "\\CarbonDMP";

        g_dataFile = g_modRoot + "\\data\\data.dat";
        g_musicDirectory = g_modRoot + "\\data\\music";

        return true;
    }

    const std::string& GetModRoot()
    {
        return g_modRoot;
    }

    const std::string& GetDataFile()
    {
        return g_dataFile;
    }

    const std::string& GetMusicDirectory()
    {
        return g_musicDirectory;
    }

    std::string GetMusicFile(const std::string& filename)
    {
        return g_musicDirectory + "\\" + filename;
    }
}