#pragma once

#include "Common.h"

namespace Nemesis::Detections::Etw
{
    inline const char* const kNtdllExports[] =
    {
        "EtwEventWrite",
        "EtwEventWriteFull",
        "EtwEventWriteEx",
        "EtwEventWriteTransfer",
        "EtwEventRegister",
        "EtwEventUnregister",
        "EtwNotificationRegister",
        "NtTraceEvent",
    };

    void CheckAll();
}
