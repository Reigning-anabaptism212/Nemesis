#include "Parser/Internal.h"

#include <cstdlib>
#include <cstring>

namespace Nemesis::Parser::Internal
{
    LPVOID ScanMemoryForPattern(LPVOID StartAddr, DWORD SearchSize, BYTE* Pattern, DWORD PatternLen)
    {
        __try
        {
            for (DWORD Offset = 0; Offset < SearchSize - PatternLen; Offset++)
            {
                BOOL Match = TRUE;
                for (DWORD Index = 0; Index < PatternLen; Index++)
                {
                    if (*((BYTE*)StartAddr + Offset + Index) != Pattern[Index])
                    {
                        Match = FALSE;
                        break;
                    }
                }
                if (Match)
                {
                    return (LPVOID)((DWORD_PTR)StartAddr + Offset);
                }
            }
            return NULL;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return NULL;
        }
    }
    //
    // scans loaded DLL for an internal function name, finds the runtimes
    // reference to that name, then extracts the associated function pointer 
    //
    LPVOID ResolveInternalFunction(const char* DllName, const char* FuncName)
    {
        HMODULE HMod = GetModuleHandleA(DllName);
        if (HMod == NULL)
        {
            return NULL;
        }

        SIZE_T NameLen = strlen(FuncName) + 1;
        BYTE* NameBuffer = (BYTE*)malloc(NameLen);
        if (!NameBuffer)
        {
            return NULL;
        }

        memcpy(NameBuffer, FuncName, NameLen);
        LPVOID NameLocation = ScanMemoryForPattern((LPVOID)HMod, UINT_MAX, NameBuffer, (DWORD)NameLen);
        free(NameBuffer);

        if (NameLocation == NULL)
        {
            return NULL;
        }

        DWORD_PTR NamePointer = (DWORD_PTR)NameLocation;
        LPVOID PointerLocation = ScanMemoryForPattern((LPVOID)HMod, UINT_MAX, (BYTE*)&NamePointer, sizeof(DWORD_PTR));

        if (PointerLocation == NULL)
        {
            return NULL;
        }

        LPVOID FunctionAddr = *(LPVOID*)((DWORD_PTR)PointerLocation - sizeof(LPVOID));

        if ((DWORD_PTR)FunctionAddr < (DWORD_PTR)HMod || (DWORD_PTR)FunctionAddr >= (DWORD_PTR)HMod + UINT_MAX)
        {
            return NULL;
        }

        return FunctionAddr;
    }
}
