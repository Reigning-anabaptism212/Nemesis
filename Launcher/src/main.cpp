#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <tlhelp32.h>

#include "Common.h"
#include "Util/Path.h"

namespace
{
    void PrintUsage(const char* Self)
    {
        Nemesis::ConsolePrintf(
            "Nemesis Launcher\n"
            "\n"
            "Usage: %s <target.exe|.bat|.cmd> [-- <args passed to target...>]\n"
            "       %s --dll <path\\to\\Nemesis.dll> <target> [-- <args...>]\n"
            "\n"
            "By default Nemesis.dll is loaded from the same directory as the launcher.\n",
            Self, Self);
    }

    std::string GetLauncherDir()
    {
        char Buf[MAX_PATH] = {};
        if (!Nemesis::Util::GetSelfDir(Buf, sizeof(Buf)))
        {
            return ".";
        }
        return Buf;
    }

    bool FileExists(const std::string& Path)
    {
        DWORD Attr = GetFileAttributesA(Path.c_str());
        return Attr != INVALID_FILE_ATTRIBUTES && !(Attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    std::string QuoteIfNeeded(const std::string& S)
    {
        if (S.empty() || S.find(' ') == std::string::npos)
        {
            return S;
        }
        return "\"" + S + "\"";
    }

    bool HasExtensionI(const std::string& Path, const char* Ext)
    {
        if (Path.size() < strlen(Ext))
        {
            return false;
        }
        std::string Tail = Path.substr(Path.size() - strlen(Ext));
        for (size_t i = 0; i < Tail.size(); ++i)
        {
            if (tolower(static_cast<unsigned char>(Tail[i])) != tolower(static_cast<unsigned char>(Ext[i])))
            {
                return false;
            }
        }
        return true;
    }

    std::string BuildCommandLine(const std::string& Target, const std::string& ExtraArgs)
    {
        char AbsTarget[MAX_PATH] = {};
        if (GetFullPathNameA(Target.c_str(), MAX_PATH, AbsTarget, nullptr) == 0)
        {
            return {};
        }

        std::string Resolved = AbsTarget;

        if (HasExtensionI(Resolved, ".bat") || HasExtensionI(Resolved, ".cmd"))
        {
            std::string Cmd = "C:\\Windows\\System32\\cmd.exe /c " + QuoteIfNeeded(Resolved);
            if (!ExtraArgs.empty())
            {
                Cmd += ' ';
                Cmd += ExtraArgs;
            }
            return Cmd;
        }

        std::string Cmd = QuoteIfNeeded(Resolved);
        if (!ExtraArgs.empty())
        {
            Cmd += ' ';
            Cmd += ExtraArgs;
        }
        return Cmd;
    }

    bool WaitForNamedEvent(const char* Prefix, DWORD Pid, DWORD TimeoutMs)
    {
        char EventName[64] = {};
        _snprintf_s(EventName, _TRUNCATE, "%s_%lu", Prefix, Pid);

        DWORD Elapsed = 0;
        while (Elapsed < TimeoutMs)
        {
            HANDLE Ev = OpenEventA(SYNCHRONIZE, FALSE, EventName);
            if (Ev != nullptr)
            {
                DWORD WaitLeft = TimeoutMs - Elapsed;
                DWORD Rc = WaitForSingleObject(Ev, WaitLeft);
                CloseHandle(Ev);
                return Rc == WAIT_OBJECT_0;
            }
            Sleep(50);
            Elapsed += 50;
        }
        return false;
    }

    bool WaitForNemesisReady(DWORD Pid, DWORD TimeoutMs = 60000)
    {
        return WaitForNamedEvent("NemesisReady", Pid, TimeoutMs);
    }

    void SuspendProcessThreads(DWORD Pid, std::vector<HANDLE>& Out)
    {
        HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (Snap == INVALID_HANDLE_VALUE)
        {
            return;
        }

        THREADENTRY32 Te = {};
        Te.dwSize = sizeof(Te);

        if (Thread32First(Snap, &Te))
        {
            do
            {
                if (Te.th32OwnerProcessID != Pid)
                {
                    continue;
                }

                HANDLE Th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, Te.th32ThreadID);
                if (Th == nullptr)
                {
                    continue;
                }

                if (SuspendThread(Th) != static_cast<DWORD>(-1))
                {
                    Out.push_back(Th);
                }
                else
                {
                    CloseHandle(Th);
                }
            }
            while (Thread32Next(Snap, &Te));
        }

        CloseHandle(Snap);
    }

    void ResumeProcessThreads(std::vector<HANDLE>& Threads)
    {
        for (HANDLE Th : Threads)
        {
            ResumeThread(Th);
            CloseHandle(Th);
        }
        Threads.clear();
    }

    bool GetProcessImagePath(DWORD Pid, char* OutPath, DWORD OutSize)
    {
        HANDLE HProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Pid);
        if (HProc == nullptr)
        {
            return false;
        }
        DWORD Size = OutSize;
        BOOL Ok = QueryFullProcessImageNameA(HProc, 0, OutPath, &Size);
        CloseHandle(HProc);
        return Ok != FALSE && OutPath[0] != '\0';
    }

    bool ShouldInjectProcess(DWORD Pid)
    {
        char Path[MAX_PATH] = {};
        if (!GetProcessImagePath(Pid, Path, MAX_PATH))
        {
            return false;
        }
        return Nemesis::Util::EndsWithI(Path, "powershell.exe") || Nemesis::Util::EndsWithI(Path, "pwsh.exe");
    }

    bool InjectDll(HANDLE HProc, DWORD Pid, const std::string& DllPath, bool FreezeTarget, DWORD ReadyTimeoutMs)
    {
        std::vector<HANDLE> Frozen;
        if (FreezeTarget)
        {
            SuspendProcessThreads(Pid, Frozen);
        }

        bool Ok = false;
        SIZE_T PathBytes = DllPath.size() + 1;

        LPVOID Remote = VirtualAllocEx(HProc, nullptr, PathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (Remote == nullptr)
        {
            Nemesis::ConsolePrintf("[-] VirtualAllocEx failed err=%lu", GetLastError());
        }
        else
        {
            SIZE_T Written = 0;
            if (!WriteProcessMemory(HProc, Remote, DllPath.c_str(), PathBytes, &Written) || Written != PathBytes)
            {
                Nemesis::ConsolePrintf("[-] WriteProcessMemory failed err=%lu", GetLastError());
                VirtualFreeEx(HProc, Remote, 0, MEM_RELEASE);
            }
            else
            {
                HMODULE HKernel32 = GetModuleHandleA("kernel32.dll");
                if (HKernel32 == nullptr)
                {
                    Nemesis::ConsolePrintf("[-] no kernel32 handle");
                    VirtualFreeEx(HProc, Remote, 0, MEM_RELEASE);
                }
                else
                {
                    LPTHREAD_START_ROUTINE LoadLibraryARoutine =
                        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(HKernel32, "LoadLibraryA"));
                    if (LoadLibraryARoutine == nullptr)
                    {
                        Nemesis::ConsolePrintf("[-] LoadLibraryA not found");
                        VirtualFreeEx(HProc, Remote, 0, MEM_RELEASE);
                    }
                    else
                    {
                        HANDLE HRemoteThread = CreateRemoteThread(HProc, nullptr, 0, LoadLibraryARoutine, Remote, 0, nullptr);
                        if (HRemoteThread == nullptr)
                        {
                            Nemesis::ConsolePrintf("[-] CreateRemoteThread failed err=%lu", GetLastError());
                            VirtualFreeEx(HProc, Remote, 0, MEM_RELEASE);
                        }
                        else
                        {
                            Nemesis::ConsolePrintf("[?] waiting for LoadLibraryA in target");
                            WaitForSingleObject(HRemoteThread, INFINITE);

                            DWORD RemoteRc = 0;
                            GetExitCodeThread(HRemoteThread, &RemoteRc);
                            CloseHandle(HRemoteThread);
                            VirtualFreeEx(HProc, Remote, 0, MEM_RELEASE);

                            if (RemoteRc == 0)
                            {
                                Nemesis::ConsolePrintf("[-] LoadLibraryA returned null in target");
                            }
                            else
                            {
                                Nemesis::ConsolePrintf("[+] dll loaded in pid=%lu base=0x%p",
                                            Pid,
                                            reinterpret_cast<LPVOID>(static_cast<ULONG_PTR>(RemoteRc)));

                                if (!WaitForNemesisReady(Pid, ReadyTimeoutMs))
                                {
                                    Nemesis::ConsolePrintf("[-] nemesis init timed out pid=%lu", Pid);
                                }
                                else
                                {
                                    Nemesis::ConsolePrintf("[+] nemesis ready pid=%lu", Pid);
                                    Ok = true;
                                }

                                if (FreezeTarget)
                                {
                                    ResumeProcessThreads(Frozen);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (FreezeTarget && !Frozen.empty())
        {
            ResumeProcessThreads(Frozen);
        }

        return Ok;
    }

    bool IsDescendantOf(DWORD Pid, DWORD Ancestor, const std::vector<std::pair<DWORD, DWORD>>& ParentOf)
    {
        DWORD Current = Pid;
        for (int Depth = 0; Depth < 64 && Current != 0; ++Depth)
        {
            if (Current == Ancestor)
            {
                return true;
            }
            DWORD Parent = 0;
            for (const auto& Entry : ParentOf)
            {
                if (Entry.first == Current)
                {
                    Parent = Entry.second;
                    break;
                }
            }
            Current = Parent;
        }
        return false;
    }

    void MonitorChildProcesses(DWORD RootPid, const std::string& DllPath, HANDLE RootHandle)
    {
        std::set<DWORD> Injected;
        Injected.insert(RootPid);

        while (WaitForSingleObject(RootHandle, 25) == WAIT_TIMEOUT)
        {
            HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (Snap == INVALID_HANDLE_VALUE)
            {
                continue;
            }

            std::vector<std::pair<DWORD, DWORD>> ParentOf;
            PROCESSENTRY32 Pe = {};
            Pe.dwSize = sizeof(Pe);

            if (Process32First(Snap, &Pe))
            {
                do
                {
                    ParentOf.emplace_back(Pe.th32ProcessID, Pe.th32ParentProcessID);
                }
                while (Process32Next(Snap, &Pe));
            }
            CloseHandle(Snap);

            for (const auto& Entry : ParentOf)
            {
                DWORD Pid = Entry.first;
                if (Injected.count(Pid) != 0)
                {
                    continue;
                }
                if (!IsDescendantOf(Pid, RootPid, ParentOf))
                {
                    continue;
                }

                if (!ShouldInjectProcess(Pid))
                {
                    continue;
                }

                char ImagePath[MAX_PATH] = {};
                GetProcessImagePath(Pid, ImagePath, MAX_PATH);

                HANDLE HChild = OpenProcess(
                    PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_SUSPEND_RESUME,
                    FALSE,
                    Pid);
                if (HChild == nullptr)
                {
                    continue;
                }

                Nemesis::ConsolePrintf("[?] injecting child pid=%lu (%s)", Pid, ImagePath[0] ? ImagePath : "?");
                InjectDll(HChild, Pid, DllPath, true, 30000);
                Injected.insert(Pid);
                CloseHandle(HChild);
            }
        }
    }
}

int main(int Argc, char** Argv)
{
    Nemesis::EnableConsoleColors();

    if (Argc < 2)
    {
        PrintUsage(Argv[0]);
        return 1;
    }

    std::string DllPath;
    std::string Target;
    std::string ExtraArgs;

    int i = 1;
    while (i < Argc)
    {
        std::string A = Argv[i];
        if (A == "--dll" && (i + 1) < Argc)
        {
            DllPath = Argv[++i];
            ++i;
            continue;
        }
        if (A == "-h" || A == "--help" || A == "/?")
        {
            PrintUsage(Argv[0]);
            return 0;
        }
        if (Target.empty())
        {
            Target = A;
            ++i;
            continue;
        }
        if (A == "--")
        {
            ++i;
            while (i < Argc)
            {
                if (!ExtraArgs.empty())
                {
                    ExtraArgs += ' ';
                }
                ExtraArgs += Argv[i++];
            }
            break;
        }
        if (!ExtraArgs.empty())
        {
            ExtraArgs += ' ';
        }
        ExtraArgs += A;
        ++i;
    }

    if (Target.empty())
    {
        PrintUsage(Argv[0]);
        return 1;
    }

    if (DllPath.empty())
    {
        DllPath = GetLauncherDir() + "\\Nemesis.dll";
    }

    if (!FileExists(DllPath))
    {
        Nemesis::ConsolePrintf("[-] dll not found %s", DllPath.c_str());
        return 2;
    }

    if (!FileExists(Target))
    {
        Nemesis::ConsolePrintf("[-] target not found %s", Target.c_str());
        return 3;
    }

    char AbsDll[MAX_PATH] = {};
    if (GetFullPathNameA(DllPath.c_str(), MAX_PATH, AbsDll, nullptr) == 0)
    {
        Nemesis::ConsolePrintf("[-] GetFullPathName failed for dll err=%lu", GetLastError());
        return 4;
    }
    DllPath = AbsDll;

    Nemesis::ConsolePrintf("[?] launcher dll %s", DllPath.c_str());
    Nemesis::ConsolePrintf("[?] target %s", Target.c_str());
    if (!ExtraArgs.empty())
    {
        Nemesis::ConsolePrintf("[?] target args %s", ExtraArgs.c_str());
    }

    std::string CmdLine = BuildCommandLine(Target, ExtraArgs);
    if (CmdLine.empty())
    {
        Nemesis::ConsolePrintf("[-] GetFullPathName failed for target err=%lu", GetLastError());
        return 4;
    }

    Nemesis::ConsolePrintf("[?] command line %s", CmdLine.c_str());

    std::vector<char> CmdBuf(CmdLine.begin(), CmdLine.end());
    CmdBuf.push_back('\0');

    STARTUPINFOA Si = {};
    Si.cb = sizeof(Si);
    PROCESS_INFORMATION Pi = {};

    if (!CreateProcessA(
            nullptr,
            CmdBuf.data(),
            nullptr, nullptr, FALSE,
            CREATE_SUSPENDED,
            nullptr, nullptr,
            &Si, &Pi))
    {
        Nemesis::ConsolePrintf("[-] CreateProcess failed err=%lu", GetLastError());
        return 5;
    }

    Nemesis::ConsolePrintf("[+] target pid %lu suspended", Pi.dwProcessId);

    if (!InjectDll(Pi.hProcess, Pi.dwProcessId, DllPath, false, 15000))
    {
        Nemesis::ConsolePrintf("[-] injection failed killing target");
        TerminateProcess(Pi.hProcess, 1);
        CloseHandle(Pi.hThread);
        CloseHandle(Pi.hProcess);
        return 6;
    }

    Nemesis::ConsolePrintf("[?] resuming target main thread");
    if (ResumeThread(Pi.hThread) == static_cast<DWORD>(-1))
    {
        Nemesis::ConsolePrintf("[-] ResumeThread failed err=%lu", GetLastError());
        TerminateProcess(Pi.hProcess, 1);
        CloseHandle(Pi.hThread);
        CloseHandle(Pi.hProcess);
        return 7;
    }

    Nemesis::ConsolePrintf("[?] watching child processes for inject");
    MonitorChildProcesses(Pi.dwProcessId, DllPath, Pi.hProcess);

    DWORD ExitCode = 0;
    GetExitCodeProcess(Pi.hProcess, &ExitCode);
    Nemesis::ConsolePrintf("[?] target exited code %lu", ExitCode);

    CloseHandle(Pi.hThread);
    CloseHandle(Pi.hProcess);
    return 0;
}
