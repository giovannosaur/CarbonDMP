#pragma once

#include <string>

enum class TrackContext
{
    FE,
    GAMEPLAY,
    BOTH
};

struct Track
{
    std::string name;
    std::string artist;
    std::string album;
    TrackContext context;
    std::string filename;
};