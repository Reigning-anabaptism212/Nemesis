#pragma once

#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#ifndef Nemesis_CLR_INSPECT_CC
#define Nemesis_CLR_INSPECT_CC
#endif

namespace Nemesis
{
    namespace Ansi
    {
        inline constexpr const char* Reset          = "\x1b[0m";
        inline constexpr const char* Bold           = "\x1b[1m";
        inline constexpr const char* Dim            = "\x1b[2m";
        inline constexpr const char* Red            = "\x1b[31m";
        inline constexpr const char* Green          = "\x1b[32m";
        inline constexpr const char* Yellow         = "\x1b[33m";
        inline constexpr const char* Blue           = "\x1b[34m";
        inline constexpr const char* Magenta        = "\x1b[35m";
        inline constexpr const char* Cyan           = "\x1b[36m";
        inline constexpr const char* BrightRed      = "\x1b[91m";
        inline constexpr const char* BrightGreen    = "\x1b[92m";
        inline constexpr const char* BrightYellow   = "\x1b[93m";
        inline constexpr const char* BrightBlue     = "\x1b[94m";
        inline constexpr const char* BrightMagenta  = "\x1b[95m";
        inline constexpr const char* BrightCyan     = "\x1b[96m";
        inline constexpr const char* DimCyan        = "\x1b[2;36m";
    }

    inline bool EnableConsoleColors()
    {
        static bool Attempted = false;
        static bool Enabled   = false;
        if (Attempted)
        {
            return Enabled;
        }
        Attempted = true;

        auto EnableOn = [](DWORD HandleId) -> bool
        {
            HANDLE H = GetStdHandle(HandleId);
            if (H == nullptr || H == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            DWORD Mode = 0;
            if (!GetConsoleMode(H, &Mode))
            {
                return false;
            }

            Mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            return SetConsoleMode(H, Mode) != FALSE;
        };

        Enabled = EnableOn(STD_OUTPUT_HANDLE) || EnableOn(STD_ERROR_HANDLE);
        return Enabled;
    }

    inline const char* SelectMessageColor(const char* Msg)
    {
        if (Msg == nullptr || Msg[0] == '\0')
        {
            return Ansi::Reset;
        }

        if (strncmp(Msg, "[+]", 3) == 0)
        {
            return Ansi::BrightGreen;
        }
        if (strncmp(Msg, "[-]", 3) == 0)
        {
            return Ansi::BrightRed;
        }
        if (strncmp(Msg, "[?]", 3) == 0)
        {
            return Ansi::BrightYellow;
        }

        return Ansi::Reset;
    }

    inline void ConsolePrintf(const char* Fmt, ...)
    {
        char Buffer[2048];
        va_list Args;
        va_start(Args, Fmt);
        int Written = _vsnprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, Fmt, Args);
        va_end(Args);
        if (Written < 0)
        {
            Buffer[sizeof(Buffer) - 1] = '\0';
        }

        if (!EnableConsoleColors())
        {
            fputs(Buffer, stdout);
            fputc('\n', stdout);
            return;
        }

        const char* Color = SelectMessageColor(Buffer);
        std::printf("%s%s%s\n", Color, Buffer, Ansi::Reset);
    }

    inline HANDLE& LogFileHandle()
    {
        static HANDLE H = INVALID_HANDLE_VALUE;
        return H;
    }

    inline void InitLogFile()
    {
        if (LogFileHandle() != INVALID_HANDLE_VALUE)
        {
            return;
        }

        char Temp[MAX_PATH] = {};
        if (GetTempPathA(MAX_PATH, Temp) == 0)
        {
            return;
        }

        char Path[MAX_PATH] = {};
        _snprintf_s(Path, _TRUNCATE, "%sNemesis.log", Temp);
        LogFileHandle() = CreateFileA(
            Path,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
    }

    inline bool GetDumpDirectory(char* Out, size_t OutSize)
    {
        if (Out == nullptr || OutSize == 0)
        {
            return false;
        }

        char Temp[MAX_PATH] = {};
        if (GetTempPathA(MAX_PATH, Temp) == 0)
        {
            Out[0] = '\0';
            return false;
        }

        _snprintf_s(Out, OutSize, _TRUNCATE, "%sNemesis_dumps", Temp);
        return Out[0] != '\0';
    }

    inline void Log(const char* Fmt, ...)
    {
        char Buffer[1024];
        va_list Args;
        va_start(Args, Fmt);
        int Written = _vsnprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, Fmt, Args);
        va_end(Args);
        if (Written < 0)
        {
            Buffer[sizeof(Buffer) - 1] = '\0';
        }

        char Line[1100];
        _snprintf_s(Line, _TRUNCATE, "[Nemesis][pid=%lu] %s\r\n", GetCurrentProcessId(), Buffer);

        OutputDebugStringA(Line);

        InitLogFile();
        if (LogFileHandle() != INVALID_HANDLE_VALUE)
        {
            DWORD Ignored = 0;
            WriteFile(LogFileHandle(), Line, static_cast<DWORD>(strlen(Line)), &Ignored, nullptr);
        }

        if (EnableConsoleColors())
        {
            const char* Color = SelectMessageColor(Buffer);
            std::printf(
                "%s[Nemesis]%s %s%s%s\n",
                Ansi::Bold,
                Ansi::Reset,
                Color,
                Buffer,
                Ansi::Reset);
        }
        else
        {
            std::printf("[Nemesis] %s\n", Buffer);
        }
    }

    inline void EnsureDumpDirectory(bool LogPath = true)
    {
        char Dir[MAX_PATH] = {};
        if (!GetDumpDirectory(Dir, sizeof(Dir)))
        {
            return;
        }

        CreateDirectoryA(Dir, nullptr);
        if (LogPath)
        {
            Log("[+] dump folder %s", Dir);
        }
    }
}

#define Nemesis_LOG(...) ::Nemesis::Log(__VA_ARGS__)
