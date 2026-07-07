#pragma once

#include "Common.h"

namespace Nemesis::Detections::Amsi
{
    inline const char* const kAmsiExports[] ={
        "AmsiInitialize",
        "AmsiUninitialize",
        "AmsiOpenSession",
        "AmsiCloseSession",
        "AmsiScanBuffer",
        "AmsiScanString",
        "AmsiNotifyOperation",
    };

    inline const char* const kClrRdataStrings[] =
    {
        "AmsiScanBuffer",
        "AmsiInitialize",
        "amsi.dll",
    };

    void CheckAll();
}
