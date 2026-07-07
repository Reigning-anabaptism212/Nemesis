#include "Hooks/ClrHooks.h"
#include "Hooks/ClrNativeInspect.h"
#include "Parser/Internal.h"
#include "Detections/CustomLoadImage.h"

#include <detours/detours.h>

#pragma comment(lib, "detours.lib")

extern "C"
{
    void NLoadImageDetour(void);
    void NLoadFileDetour(void);
    void AssemblyNativeLoadFromBufferDetour(void);
    void ClrJitCompileMethodDetour(void);
}

namespace Nemesis::Hooks::Clr
{
    namespace
    {
        HookRecord g_Hooks[] =
        {
            // very ugly,fixing soon...
            { "clr.dll","nLoadImage",nullptr,&Nemesis::Detections::CustomLoadImage::SetNLoadImageBase, (LPVOID)&NLoadImageDetour,&OrgNLoadImage,nullptr,FALSE },
            { "clr.dll","nLoadFile",nullptr,nullptr,(LPVOID)&NLoadFileDetour,&OrgNLoadFile,nullptr,FALSE },
            { "clr.dll","AssemblyNative::LoadFromBuffer",&Nemesis::Detections::CustomLoadImage::ResolveLoadFromBuffer, nullptr,(LPVOID)&AssemblyNativeLoadFromBufferDetour,&OrgAssemblyNativeLoadFromBuffer,nullptr,FALSE },
            { "clrjit.dll", "CompileMethod",nullptr,nullptr,(LPVOID)&ClrJitCompileMethodDetour,&OrgClrJitCompileMethod,nullptr,FALSE },
        };
    }

    bool InstallAll()
    {
        Nemesis_LOG("[?] looking for clr internals");

        LONG Rc = DetourTransactionBegin();
        if (Rc != NO_ERROR)
        {
            Nemesis_LOG("[-] detour begin failed rc=%ld", Rc);
            return false;
        }

        DetourUpdateThread(GetCurrentThread());

        int Queued = 0;
        for (size_t i = 0; i < _countof(g_Hooks); ++i)
        {
            HookRecord& H = g_Hooks[i];
            if (H.Attached)
            {
                continue;
            }

            LPVOID Target = nullptr;
            if (H.Resolver != nullptr)
            {
                Target = H.Resolver();
            }
            else
            {
                Target = Nemesis::Parser::Internal::ResolveInternalFunction(H.Module, H.Symbol);
            }

            if (Target == nullptr)
            {
                Nemesis_LOG("[?] skipped %s!%s couldnt find it", H.Module, H.Symbol);
                continue;
            }

            if (H.PostResolve != nullptr)
            {
                H.PostResolve(Target);
            }

            *H.OrgSlot = Target;
            H.HookTarget = Target;

            Rc = DetourAttach(H.OrgSlot, H.DetourFn);
            if (Rc != NO_ERROR)
            {
                Nemesis_LOG("[-] couldnt attach %s rc=%ld", H.Symbol, Rc);
                *H.OrgSlot = nullptr;
                continue;
            }

            H.Attached = TRUE;
            ++Queued;
            Nemesis_LOG("[+] queued hook %s @0x%p detour @0x%p", H.Symbol, Target, H.DetourFn);
        }

        if (Queued == 0)
        {
            DetourTransactionAbort();
            Nemesis_LOG("[-] nothing to hook");
            return false;
        }

        Rc = DetourTransactionCommit();
        if (Rc != NO_ERROR)
        {
            Nemesis_LOG("[-] detour commit failed rc=%ld", Rc);
            for (auto& H : g_Hooks)
            {
                H.Attached = FALSE;
                *H.OrgSlot = nullptr;
                H.HookTarget = nullptr;
            }
            Nemesis::Detections::CustomLoadImage::SetNLoadImageBase(nullptr);
            return false;
        }

        Nemesis_LOG("[+] attached %d hooks", Queued);
        for (size_t i = 0; i < _countof(g_Hooks); ++i)
        {
            HookRecord& H = g_Hooks[i];
            if (H.Attached && H.HookTarget != nullptr)
            {
                FlushInstructionCache(GetCurrentProcess(), H.HookTarget, 32);
            }
        }
        return true;
    }

    bool RestoreAll()
    {
        LONG Rc = DetourTransactionBegin();
        if (Rc != NO_ERROR)
        {
            Nemesis_LOG("[-] restore detour begin failed rc=%ld", Rc);
            return false;
        }
        DetourUpdateThread(GetCurrentThread());

        int Queued = 0;
        for (auto& H : g_Hooks)
        {
            if (!H.Attached)
            {
                continue;
            }

            Rc = DetourDetach(H.OrgSlot, H.DetourFn);
            if (Rc != NO_ERROR)
            {
                Nemesis_LOG("[-] couldnt detach %s rc=%ld", H.Symbol, Rc);
                continue;
            }
            ++Queued;
            Nemesis_LOG("[+] queued detach %s", H.Symbol);
        }

        if (Queued == 0)
        {
            DetourTransactionAbort();
            return false;
        }

        Rc = DetourTransactionCommit();
        if (Rc != NO_ERROR)
        {
            Nemesis_LOG("[-] restore detour commit failed rc=%ld", Rc);
            return false;
        }

        for (auto& H : g_Hooks)
        {
            H.Attached = FALSE;
            *H.OrgSlot = nullptr;
            H.HookTarget = nullptr;
        }
        Nemesis::Detections::CustomLoadImage::SetNLoadImageBase(nullptr);

        Nemesis_LOG("[+] detached %d hooks", Queued);
        return true;
    }
}
