#pragma once

#include <string>

namespace Paths
{
    bool Init();

    const std::string& GetModRoot();
    const std::string& GetDataFile();
    const std::string& GetMusicDirectory();

    std::string GetMusicFile(const std::string& filename);
}