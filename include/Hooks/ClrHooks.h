#pragma once

#include "Common.h"

namespace Nemesis::Hooks::Clr
{
    using ResolverFn = LPVOID(*)();
    using PostResolveFn = void(*)(LPVOID);

    struct HookRecord
    {
        const char*   Module;
        const char*   Symbol;
        ResolverFn    Resolver;
        PostResolveFn PostResolve;
        LPVOID        DetourFn;
        PVOID*        OrgSlot;
        LPVOID        HookTarget;
        BOOL          Attached;
    };

    bool InstallAll();
    bool RestoreAll();
}
