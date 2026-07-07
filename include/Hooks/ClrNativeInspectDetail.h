#pragma once

#include "Hooks/ClrNativeInspect.h"

namespace Nemesis::Hooks::ClrNativeInspect
{
    extern LONG          g_DumpCounter;
    extern volatile LONG g_HitNLoadImage;
    extern volatile LONG g_HitNLoadFile;
    extern volatile LONG g_HitLoadFromBuffer;
    extern InspectDiagSlot g_DiagSlots[kDiagSlots];
    extern volatile LONG g_DiagSeq;
    extern HANDLE        g_DumpWakeEvent;

    namespace Detail
    {
        struct QueuedInspect
        {
            volatile LONG Ready;
            char          Tag[24];
            PVOID         Arg0;
            PVOID         Arg1;
            PVOID         Arg2;
            PVOID         Arg3;
            INT32         LenHint;
        };

        inline constexpr int kInspectQueueSlots = 16;

        extern QueuedInspect g_InspectQueue[kInspectQueueSlots];
        extern QueuedDump    g_DumpQueue[kDumpQueueSlots];
        extern HANDLE        g_DumpWorkerThread;

        void InspectNLoadImage(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3);
        void RecordInspectDiag(const char* Tag, PVOID Arg0, PVOID Arg1, PVOID Arg2, DWORD LenHint);
        void TryDumpRawOrManaged(PVOID Base, DWORD SizeHint, const char* Tag);
        void QueueInspectWork(const char* Tag, PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3, INT32 LenHint);
        void FlushInspectQueue();
        void FlushPendingDumps();
        DWORD WINAPI DumpWorkerMain(LPVOID);
    }
}
