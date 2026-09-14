#pragma once

#include "track.h"

#include <string>
#include <vector>

namespace Playlist
{
    bool LoadFromFile(const std::string& path);

    bool Save();

    bool SetTrackContext(
        size_t trackIndex,
        TrackContext context
    );

    std::vector<size_t> GetTracksForContext(
        TrackContext context
    );

    const std::vector<Track>& GetTracks();

    std::vector<Track>& GetTracksMutable();

    void Clear();
}
