#pragma once

#include "Common.h"

namespace Nemesis::Detections::ExportCompare
{
    inline constexpr DWORD kProbeBytes = 32;

    enum class LoadPolicy
    {
        TryLoad,
        MustBeLoaded,
    };

    void CheckExport(const char* Tag, const char* ModuleName, const char* ExportName, LoadPolicy Policy);
}
