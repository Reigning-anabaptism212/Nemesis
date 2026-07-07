#include "Init/Runtime.h"

#include "Common.h"
#include "Hooks/ClrHooks.h"
#include "Hooks/ClrNativeInspect.h"
#include "Detections/Amsi/Amsi.h"
#include "Detections/Etw/Etw.h"
#include "Util/Path.h"

namespace
{
    HANDLE g_ReadyEvent = nullptr;

    bool IsHostWithoutClr()
    {
        char Path[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, Path, MAX_PATH) == 0)
        {
            return false;
        }

        return Nemesis::Util::EndsWithI(Path, "cmd.exe") || Nemesis::Util::EndsWithI(Path, "conhost.exe");
    }

    DWORD WINAPI InitThreadMain(LPVOID)
    {
        Nemesis::EnableConsoleColors();
        Nemesis::InitLogFile();
        ClrNativeInspect_StartDumpWorker();
        Nemesis::EnsureDumpDirectory();
        Nemesis::Detections::Amsi::CheckAll();
        Nemesis::Detections::Etw::CheckAll();

        bool Hooked = false;
        for (int Attempt = 0; Attempt < 800; ++Attempt)
        {
            if (GetModuleHandleA("clr.dll") != nullptr)
            {
                if (Nemesis::Hooks::Clr::InstallAll())
                {
                    Hooked = true;
                    break;
                }
            }
            else if (Attempt > 40 && IsHostWithoutClr())
            {
                break;
            }

            Sleep(25);
        }

        if (!Hooked && GetModuleHandleA("clr.dll") != nullptr)
        {
            Nemesis_LOG("[?] clr loaded but hooks never stuck");
        }

        if (g_ReadyEvent != nullptr && (Hooked || IsHostWithoutClr()))
        {
            SetEvent(g_ReadyEvent);
        }
        else if (!Hooked && GetModuleHandleA("clr.dll") != nullptr)
        {
            Nemesis_LOG("[?] clr here but hooks didnt stick wont tell launcher im ready");
        }

        return 0;
    }
}

namespace Nemesis::Init
{
    void OnAttach(HMODULE hModule)
    {
        DisableThreadLibraryCalls(hModule);

        char EventName[64] = {};
        _snprintf_s(EventName, _TRUNCATE, "NemesisRdy_%lu", GetCurrentProcessId());
        g_ReadyEvent = CreateEventA(nullptr, TRUE, FALSE, EventName);

        HANDLE Th = CreateThread(nullptr, 0, InitThreadMain, nullptr, 0, nullptr);
        if (Th != nullptr)
        {
            CloseHandle(Th);
        }
    }

    void OnDetach()
    {
        ClrNativeInspect_FlushPendingDumps();
        Nemesis::Hooks::Clr::RestoreAll();

        if (g_ReadyEvent != nullptr)
        {
            CloseHandle(g_ReadyEvent);
            g_ReadyEvent = nullptr;
        }
    }
}
