#include "Detections/Etw/Etw.h"
#include "Detections/ExportCompare.h"

namespace Nemesis::Detections::Etw
{
    void CheckAll()
    {
        for (const char* Name : kNtdllExports)
        {
            ExportCompare::CheckExport("etw", "ntdll.dll", Name, ExportCompare::LoadPolicy::MustBeLoaded);
        }
    }
}
