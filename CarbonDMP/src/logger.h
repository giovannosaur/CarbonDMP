#pragma once

namespace Logger
{
    bool Init();
    void Shutdown();

#ifdef CARBONDMP_DEBUG
    void Info(const char* message);
#else
    inline void Info(const char*)
    {
    }
#endif

    void Error(const char* message);
}