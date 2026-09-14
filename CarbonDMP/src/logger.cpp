#include "pch.h"

#include "logger.h"

#include <cstdio>
#include <windows.h>

namespace
{
    FILE* g_logFile = nullptr;
}

namespace Logger
{
    bool Init()
    {
        fopen_s(&g_logFile, "CarbonDMP.log", "a");

        if (!g_logFile)
            return false;

        fprintf(
            g_logFile,
            "\n=== CarbonDMP started ===\n"
        );

        fflush(g_logFile);

        return true;
    }

    void Shutdown()
    {
        if (g_logFile)
        {
            fprintf(g_logFile, "=== CarbonDMP shutdown ===\n");
            fclose(g_logFile);
            g_logFile = nullptr;
        }
    }

    #ifdef CARBONDMP_DEBUG

    void Info(const char* message)
    {
        if (!g_logFile)
            return;

        fprintf(g_logFile, "[INFO] %s\n", message);
        fflush(g_logFile);
    }

    #endif

    void Error(const char* message)
    {
        if (!g_logFile)
            return;

        fprintf(g_logFile, "[ERROR] %s\n", message);
        fflush(g_logFile);
    }
}