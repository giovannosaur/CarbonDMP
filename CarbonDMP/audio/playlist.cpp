#include "pch.h"

#include "playlist.h"

#include "../src/logger.h"

#include <fstream>
#include <sstream>

namespace
{
    std::vector<Track> g_tracks;
    std::string g_loadedPath;

    std::string Trim(const std::string& text)
    {
        const size_t first =
            text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return "";

        const size_t last =
            text.find_last_not_of(" \t\r\n");

        return text.substr(
            first,
            last - first + 1
        );
    }

    bool ParseContext(
        const std::string& text,
        TrackContext& context
    )
    {
        const std::string value = Trim(text);

        if (value == "FE" ||
            value == "MENU")
        {
            context = TrackContext::FE;
            return true;
        }

        if (value == "GAMEPLAY")
        {
            context = TrackContext::GAMEPLAY;
            return true;
        }

        if (value == "BOTH")
        {
            context = TrackContext::BOTH;
            return true;
        }

        return false;
    }

    const char* ContextToString(
        TrackContext context
    )
    {
        switch (context)
        {
        case TrackContext::FE:
            return "FE";

        case TrackContext::GAMEPLAY:
            return "GAMEPLAY";

        case TrackContext::BOTH:
            return "BOTH";

        default:
            return "BOTH";
        }
    }
}

namespace Playlist
{
    bool LoadFromFile(const std::string& path)
    {
        g_tracks.clear();

        std::ifstream file(path);

        if (!file.is_open())
        {
            Logger::Error(
                "Failed to open data.dat."
            );

            return false;
        }

        g_loadedPath = path;

        std::string line;
        int lineNumber = 0;

        while (std::getline(file, line))
        {
            lineNumber++;

            line = Trim(line);

            if (line.empty())
                continue;

            if (line[0] == '#')
                continue;

            std::stringstream stream(line);

            std::string name;
            std::string artist;
            std::string album;
            std::string contextText;
            std::string filename;

            if (!std::getline(
                stream,
                name,
                '|') ||
                !std::getline(
                    stream,
                    artist,
                    '|') ||
                !std::getline(
                    stream,
                    album,
                    '|') ||
                !std::getline(
                    stream,
                    contextText,
                    '|') ||
                !std::getline(
                    stream,
                    filename))
            {
                Logger::Error(
                    "Invalid line in data.dat."
                );

                continue;
            }

            Track track;

            track.name = Trim(name);
            track.artist = Trim(artist);
            track.album = Trim(album);
            track.filename = Trim(filename);

            if (!ParseContext(
                contextText,
                track.context))
            {
                Logger::Error(
                    "Invalid context in data.dat."
                );

                continue;
            }

            g_tracks.push_back(track);
        }

        Logger::Info(
            "data.dat loaded successfully."
        );

        return true;
    }

    bool Save()
    {
        if (g_loadedPath.empty())
        {
            Logger::Error(
                "Playlist: cannot save because no data.dat path is loaded."
            );

            return false;
        }

        std::ofstream file(
            g_loadedPath,
            std::ios::out |
            std::ios::trunc
        );

        if (!file.is_open())
        {
            Logger::Error(
                "Playlist: failed to open data.dat for writing."
            );

            return false;
        }

        for (const Track& track : g_tracks)
        {
            file
                << track.name << " | "
                << track.artist << " | "
                << track.album << " | "
                << ContextToString(track.context) << " | "
                << track.filename
                << '\n';
        }

        if (!file.good())
        {
            Logger::Error(
                "Playlist: failed while writing data.dat."
            );

            return false;
        }

        Logger::Info(
            "Playlist: data.dat saved successfully."
        );

        return true;
    }

    bool SetTrackContext(
        size_t trackIndex,
        TrackContext context
    )
    {
        if (trackIndex >= g_tracks.size())
        {
            Logger::Error(
                "Playlist: invalid track index."
            );

            return false;
        }

        g_tracks[trackIndex].context = context;
        return true;
    }

    std::vector<size_t> GetTracksForContext(
        TrackContext context
    )
    {
        std::vector<size_t> result;

        for (size_t i = 0; i < g_tracks.size(); ++i)
        {
            const TrackContext trackContext =
                g_tracks[i].context;

            if (trackContext == context ||
                trackContext == TrackContext::BOTH)
            {
                result.push_back(i);
            }
        }

        return result;
    }

    const std::vector<Track>& GetTracks()
    {
        return g_tracks;
    }

    std::vector<Track>& GetTracksMutable()
    {
        return g_tracks;
    }

    void Clear()
    {
        g_tracks.clear();
        g_loadedPath.clear();
    }
}
