#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Init/Runtime.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    (void)lpReserved;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        Nemesis::Init::OnAttach(hModule);
        break;
    case DLL_PROCESS_DETACH:
        Nemesis::Init::OnDetach();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}
