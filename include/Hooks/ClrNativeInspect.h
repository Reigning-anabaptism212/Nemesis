#pragma once

#include "Common.h"

namespace Nemesis::Hooks::ClrNativeInspect
{
    inline constexpr DWORD kMaxPeSize = 128u * 1024u * 1024u;
    inline constexpr int   kDiagSlots = 16;
    inline constexpr int   kDumpQueueSlots = 4;

    inline constexpr DWORD kMaxObjectScanOffset = 0x48;
    inline constexpr DWORD kPointerScanStep = sizeof(PVOID);
    inline constexpr DWORD kInlineScanStep = 4;
    inline constexpr DWORD kDumpQueueDataBytes = 4u * 1024u * 1024u;

    struct InspectDiagSlot
    {
        volatile LONG Ready;
        char          Tag[24];
        PVOID         Arg0;
        PVOID         Arg1;
        PVOID         Arg2;
        DWORD         LenHint;
        BYTE          Preview[32];
    };

    struct PeInfo
    {
        const BYTE* Base;
        DWORD       FileSize;
    };

    struct ResolvedBuffer
    {
        const BYTE* Data;
        DWORD       Length;
        BOOL        Found;
    };

    struct QueuedDump
    {
        volatile LONG Ready;
        DWORD         Size;
        char          Tag[64];
        BYTE          Data[kDumpQueueDataBytes];
    };

    void WriteDump(const BYTE* Data, DWORD Size, const char* Tag);
    void FlushDiagLogs();
    BOOL TryReadLengthAt(const BYTE* Obj, DWORD Offset, DWORD* OutLen);
    void TryDumpPE(PVOID Base, const char* Tag);
    void TryDumpRawPE(PVOID Base, DWORD SizeHint, const char* Tag);
    void TryDumpFromManagedArg(PVOID Arg, const char* Tag);
}

extern "C"
{
    void ClrNativeInspect_StartDumpWorker();
    void ClrNativeInspect_FlushPendingDumps();

    VOID Nemesis_CLR_INSPECT_CC ClrNative_NLoadImage_Inspect(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3);
    VOID Nemesis_CLR_INSPECT_CC ClrNative_NLoadFile_Inspect(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3);
    VOID Nemesis_CLR_INSPECT_CC ClrNative_LoadFromBuffer_Inspect(PVOID Arg0, PVOID RawAssemblyBuffer, INT32 RawAssemblyLength, PVOID Arg3, PVOID CallerReturnAddress);
    VOID Nemesis_CLR_INSPECT_CC ClrJit_CompileMethod_Inspect(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3);

    extern PVOID OrgNLoadImage;
    extern PVOID OrgNLoadFile;
    extern PVOID OrgAssemblyNativeLoadFromBuffer;
    extern PVOID OrgClrJitCompileMethod;
}
