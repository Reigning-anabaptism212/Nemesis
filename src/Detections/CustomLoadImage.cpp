#include "Detections/CustomLoadImage.h"

namespace Nemesis::Detections::CustomLoadImage
{
    static LPVOID g_NLoadImageBase = nullptr;

    void SetNLoadImageBase(LPVOID Base)
    {
        g_NLoadImageBase = Base;
    }

#ifdef _WIN64
    static bool GetFunctionBounds(LPVOID Address, BYTE** OutStart, BYTE** OutEnd)
    {
        if (Address == nullptr)
        {
            return false;
        }

        HMODULE HMod = nullptr;
        if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(Address),
                &HMod
            ) || HMod == nullptr)
        {
            return false;
        }

        BYTE* ModBase = reinterpret_cast<BYTE*>(HMod);

        __try
        {
            IMAGE_DOS_HEADER* Dos = reinterpret_cast<IMAGE_DOS_HEADER*>(ModBase);
            if (Dos->e_magic != IMAGE_DOS_SIGNATURE)
            {
                return false;
            }

            IMAGE_NT_HEADERS* Nt = reinterpret_cast<IMAGE_NT_HEADERS*>(ModBase + Dos->e_lfanew);
            if (Nt->Signature != IMAGE_NT_SIGNATURE)
            {
                return false;
            }

            IMAGE_DATA_DIRECTORY& Dir = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
            if (Dir.VirtualAddress == 0 || Dir.Size < sizeof(RUNTIME_FUNCTION))
            {
                return false;
            }

            RUNTIME_FUNCTION* Table = reinterpret_cast<RUNTIME_FUNCTION*>(ModBase + Dir.VirtualAddress);
            DWORD Count = Dir.Size / sizeof(RUNTIME_FUNCTION);
            DWORD TargetRVA = static_cast<DWORD>(reinterpret_cast<BYTE*>(Address) - ModBase);

            DWORD Lo = 0;
            DWORD Hi = Count;
            while (Lo < Hi)
            {
                DWORD Mid = Lo + (Hi - Lo) / 2;
                if (TargetRVA < Table[Mid].BeginAddress)
                {
                    Hi = Mid;
                }
                else if (TargetRVA >= Table[Mid].EndAddress)
                {
                    Lo = Mid + 1;
                }
                else
                {
                    *OutStart = ModBase + Table[Mid].BeginAddress;
                    *OutEnd   = ModBase + Table[Mid].EndAddress;
                    return true;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        return false;
    }
#endif

    LPVOID ResolveLoadFromBuffer()
    {
        if (g_NLoadImageBase == nullptr)
        {
            return nullptr;
        }

#ifdef _WIN64
        BYTE* FuncStart = nullptr;
        BYTE* FuncEnd   = nullptr;
        if (!GetFunctionBounds(g_NLoadImageBase, &FuncStart, &FuncEnd))
        {
            return nullptr;
        }

        if (FuncStart == nullptr || FuncEnd == nullptr || FuncEnd <= FuncStart + 10)
        {
            return nullptr;
        }

        BYTE* Limit = FuncEnd - 10;
        for (BYTE* P = FuncStart; P < Limit; ++P)
        {
            LPVOID Found = nullptr;
            __try
            {
                if (P[0] == 0x48 && P[1] == 0x89 && P[2] == 0x44 && P[3] == 0x24 && P[5] == 0xE8)
                {
                    INT32 Rel = *reinterpret_cast<INT32*>(P + 6);
                    Found = P + 10 + Rel;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return nullptr;
            }

            if (Found != nullptr)
            {
                return Found;
            }
        }
#endif
        return nullptr;
    }
}
