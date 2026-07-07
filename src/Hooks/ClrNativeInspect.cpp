#include "Hooks/ClrNativeInspect.h"
#include "Hooks/ClrNativeInspectDetail.h"
#include "Common.h"

#include <cstdio>
#include <cstring>

extern "C"
{
    PVOID OrgNLoadImage = nullptr;
    PVOID OrgNLoadFile = nullptr;
    PVOID OrgAssemblyNativeLoadFromBuffer = nullptr;
    PVOID OrgClrJitCompileMethod = nullptr;
}

namespace Nemesis::Hooks::ClrNativeInspect
{
    LONG          g_DumpCounter = 0;
    volatile LONG g_HitNLoadImage = 0;
    volatile LONG g_HitNLoadFile = 0;
    volatile LONG g_HitLoadFromBuffer = 0;
    InspectDiagSlot g_DiagSlots[kDiagSlots] = {};
    volatile LONG g_DiagSeq = 0;
    HANDLE        g_DumpWakeEvent = nullptr;

    namespace Detail
    {
        QueuedInspect g_InspectQueue[kInspectQueueSlots] = {};
        QueuedDump    g_DumpQueue[kDumpQueueSlots] = {};
        HANDLE        g_DumpWorkerThread = nullptr;

        void QueueInspectWork(const char* Tag, PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3, INT32 LenHint)
        {
            if (Tag == nullptr)
            {
                return;
            }

            for (int i = 0; i < kInspectQueueSlots; ++i)
            {
                QueuedInspect* Slot = &g_InspectQueue[i];
                if (InterlockedCompareExchange(&Slot->Ready, -1, 0) != 0)
                {
                    continue;
                }

                _snprintf_s(Slot->Tag, _TRUNCATE, "%s", Tag);
                Slot->Arg0 = Arg0;
                Slot->Arg1 = Arg1;
                Slot->Arg2 = Arg2;
                Slot->Arg3 = Arg3;
                Slot->LenHint = LenHint;
                InterlockedExchange(&Slot->Ready, 1);
                if (g_DumpWakeEvent != nullptr)
                {
                    SetEvent(g_DumpWakeEvent);
                }
                return;
            }
        }

        void FlushInspectQueue()
        {
            for (int i = 0; i < kInspectQueueSlots; ++i)
            {
                QueuedInspect* Slot = &g_InspectQueue[i];
                if (InterlockedCompareExchange(&Slot->Ready, 2, 1) != 1)
                {
                    continue;
                }

                if (strcmp(Slot->Tag, "nLoadImage") == 0)
                {
                    InspectNLoadImage(Slot->Arg0, Slot->Arg1, Slot->Arg2, Slot->Arg3);
                }
                else if (strcmp(Slot->Tag, "LoadFromBuffer") == 0)
                {
                    InterlockedIncrement(&g_HitLoadFromBuffer);
                    RecordInspectDiag(
                        "LoadFromBuffer",
                        Slot->Arg1,
                        Slot->Arg0,
                        Slot->Arg3,
                        Slot->LenHint > 0 ? static_cast<DWORD>(Slot->LenHint) : 0);

                    if (Slot->LenHint > 0)
                    {
                        TryDumpRawOrManaged(Slot->Arg1, static_cast<DWORD>(Slot->LenHint), "LoadFromBuffer");
                    }
                    else
                    {
                        Nemesis::Hooks::ClrNativeInspect::TryDumpFromManagedArg(Slot->Arg1, "LoadFromBuffer");
                        Nemesis::Hooks::ClrNativeInspect::TryDumpFromManagedArg(Slot->Arg0, "LoadFromBuffer_arg0");
                    }
                }

                InterlockedExchange(&Slot->Ready, 0);
            }
        }

        PeInfo ValidatePE(const BYTE* Base)
        {
            PeInfo Out{ nullptr, 0 };
            if (Base == nullptr)
            {
                return Out;
            }

            __try
            {
                if (Base[0] != 'M' || Base[1] != 'Z')
                {
                    return Out;
                }

                const IMAGE_DOS_HEADER* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Base);
                if (Dos->e_lfanew <= 0 || Dos->e_lfanew > 0x2000)
                {
                    return Out;
                }

                const IMAGE_NT_HEADERS* Nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(Base + Dos->e_lfanew);
                if (Nt->Signature != IMAGE_NT_SIGNATURE)
                {
                    return Out;
                }

                WORD NumSections = Nt->FileHeader.NumberOfSections;
                if (NumSections == 0 || NumSections > 96)
                {
                    return Out;
                }

                const BYTE* SectionsBase =
                    reinterpret_cast<const BYTE*>(Nt)
                    + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader)
                    + Nt->FileHeader.SizeOfOptionalHeader;

                const IMAGE_SECTION_HEADER* Sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(SectionsBase);

                DWORD MaxEnd = static_cast<DWORD>(Dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS);
                for (WORD i = 0; i < NumSections; ++i)
                {
                    DWORD End = Sections[i].PointerToRawData + Sections[i].SizeOfRawData;
                    if (End > MaxEnd)
                    {
                        MaxEnd = End;
                    }
                }

                if (MaxEnd == 0 || MaxEnd > kMaxPeSize)
                {
                    return Out;
                }

                Out.Base = Base;
                Out.FileSize = MaxEnd;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                Out.Base = nullptr;
                Out.FileSize = 0;
            }
            return Out;
        }

        PeInfo ScanForPE(PVOID Base)
        {
            static const int kOffsets[] = { 0, 4, 8, 12, 16, 20, 24, 32 };

            if (Base == nullptr)
            {
                return { nullptr, 0 };
            }

            for (int i = 0; i < static_cast<int>(sizeof(kOffsets) / sizeof(kOffsets[0])); ++i)
            {
                PeInfo Info = ValidatePE(reinterpret_cast<const BYTE*>(Base) + kOffsets[i]);
                if (Info.Base != nullptr)
                {
                    return Info;
                }
            }

            PVOID Deref = nullptr;
            __try
            {
                Deref = *reinterpret_cast<PVOID*>(Base);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                Deref = nullptr;
            }

            if (Deref != nullptr)
            {
                for (int i = 0; i < static_cast<int>(sizeof(kOffsets) / sizeof(kOffsets[0])); ++i)
                {
                    PeInfo Info = ValidatePE(reinterpret_cast<const BYTE*>(Deref) + kOffsets[i]);
                    if (Info.Base != nullptr)
                    {
                        return Info;
                    }
                }
            }

            return { nullptr, 0 };
        }

        void SafeCopyToBuffer(BYTE* Dst, const BYTE* Src, DWORD Size, DWORD* OutCopied)
        {
            DWORD Copied = 0;
            __try
            {
                memcpy(Dst, Src, Size);
                Copied = Size;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                Copied = 0;
            }
            *OutCopied = Copied;
        }

        BOOL HasMzMagic(const BYTE* Base)
        {
            if (Base == nullptr)
            {
                return FALSE;
            }

            __try
            {
                return Base[0] == 'M' && Base[1] == 'Z';
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return FALSE;
            }
        }

        BOOL TryReadPointerAt(const BYTE* Obj, DWORD Offset, const BYTE** OutPtr)
        {
            if (Obj == nullptr || OutPtr == nullptr)
            {
                return FALSE;
            }

            __try
            {
                ULONG_PTR Val = reinterpret_cast<ULONG_PTR>(*reinterpret_cast<const PVOID*>(Obj + Offset));
                if (Val < 0x10000)
                {
                    return FALSE;
                }
                *OutPtr = reinterpret_cast<const BYTE*>(Val);
                return TRUE;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return FALSE;
            }
        }

        BOOL TryReadBestLength(const BYTE* Obj, DWORD* OutLen)
        {
            static const DWORD kLengthOffsets[] = { 8, 0, 4, 12, 16, 24, 32, 40 };

            if (Obj == nullptr || OutLen == nullptr)
            {
                return FALSE;
            }

            for (int i = 0; i < static_cast<int>(sizeof(kLengthOffsets) / sizeof(kLengthOffsets[0])); ++i)
            {
                DWORD Len = 0;
                if (TryReadLengthAt(Obj, kLengthOffsets[i], &Len))
                {
                    *OutLen = Len;
                    return TRUE;
                }
            }

            return FALSE;
        }

        ResolvedBuffer ResolveManagedBuffer(PVOID Arg, int Depth)
        {
            ResolvedBuffer Out = { nullptr, 0, FALSE };
            if (Arg == nullptr || Depth > 2)
            {
                return Out;
            }

            const BYTE* Obj = reinterpret_cast<const BYTE*>(Arg);

            DWORD ArrayLen = 0;
            const BOOL HasLen = TryReadBestLength(Obj, &ArrayLen);

            for (DWORD Off = 0; Off <= kMaxObjectScanOffset; Off += kInlineScanStep)
            {
                PeInfo Info = ValidatePE(Obj + Off);
                if (Info.Base == nullptr)
                {
                    continue;
                }

                Out.Data = Info.Base;
                Out.Length = Info.FileSize;
                if (HasLen && ArrayLen < Out.Length)
                {
                    Out.Length = ArrayLen;
                }
                Out.Found = TRUE;
                return Out;
            }

            if (HasLen)
            {
                for (DWORD Off = 0; Off <= kMaxObjectScanOffset; Off += 2)
                {
                    if (!HasMzMagic(Obj + Off))
                    {
                        continue;
                    }

                    PeInfo Info = ValidatePE(Obj + Off);
                    if (Info.Base != nullptr)
                    {
                        Out.Data = Info.Base;
                        Out.Length = (ArrayLen < Info.FileSize) ? ArrayLen : Info.FileSize;
                        Out.Found = TRUE;
                        return Out;
                    }

                    Out.Data = Obj + Off;
                    Out.Length = ArrayLen;
                    Out.Found = TRUE;
                    return Out;
                }
            }

            for (DWORD Off = 0; Off <= kMaxObjectScanOffset; Off += kPointerScanStep)
            {
                const BYTE* Target = nullptr;
                if (!TryReadPointerAt(Obj, Off, &Target))
                {
                    continue;
                }

                PeInfo Direct = ValidatePE(Target);
                if (Direct.Base != nullptr)
                {
                    Out.Data = Direct.Base;
                    Out.Length = Direct.FileSize;
                    if (HasLen && ArrayLen < Out.Length)
                    {
                        Out.Length = ArrayLen;
                    }
                    Out.Found = TRUE;
                    return Out;
                }

                ResolvedBuffer Inner = ResolveManagedBuffer(const_cast<BYTE*>(Target), Depth + 1);
                if (Inner.Found)
                {
                    return Inner;
                }
            }

            PeInfo Scanned = ScanForPE(Arg);
            if (Scanned.Base != nullptr)
            {
                Out.Data = Scanned.Base;
                Out.Length = Scanned.FileSize;
                if (HasLen && ArrayLen < Out.Length)
                {
                    Out.Length = ArrayLen;
                }
                Out.Found = TRUE;
            }

            return Out;
        }

        void QueueDump(const BYTE* Data, DWORD Size, const char* Tag)
        {
            if (Data == nullptr || Size == 0 || Tag == nullptr)
            {
                return;
            }

            const DWORD CopySize = Size > sizeof(g_DumpQueue[0].Data)
                ? static_cast<DWORD>(sizeof(g_DumpQueue[0].Data))
                : Size;

            for (int i = 0; i < kDumpQueueSlots; ++i)
            {
                QueuedDump* Slot = &g_DumpQueue[i];
                if (InterlockedCompareExchange(&Slot->Ready, 0, 0) != 0)
                {
                    continue;
                }

                if (InterlockedCompareExchange(&Slot->Ready, -1, 0) != 0)
                {
                    continue;
                }

                DWORD Copied = 0;
                SafeCopyToBuffer(Slot->Data, Data, CopySize, &Copied);
                if (Copied == 0)
                {
                    InterlockedExchange(&Slot->Ready, 0);
                    return;
                }

                Slot->Size = Copied;
                _snprintf_s(Slot->Tag, _TRUNCATE, "%s", Tag);
                InterlockedExchange(&Slot->Ready, 1);
                SetEvent(g_DumpWakeEvent);
                return;
            }
        }

        void QueueDumpSized(const BYTE* Data, DWORD Size, const char* Tag)
        {
            if (Data == nullptr || Size == 0 || Tag == nullptr)
            {
                return;
            }

            DWORD DumpSize = Size;
            if (DumpSize > sizeof(g_DumpQueue[0].Data))
            {
                DumpSize = static_cast<DWORD>(sizeof(g_DumpQueue[0].Data));
            }

            QueueDump(Data, DumpSize, Tag);
        }

        DWORD LengthHintFromArg(PVOID Arg)
        {
            if (Arg == nullptr)
            {
                return 0;
            }

            ULONG_PTR Raw = reinterpret_cast<ULONG_PTR>(Arg);
            if (Raw >= 0x10000 && Raw <= kMaxPeSize)
            {
                return static_cast<DWORD>(Raw);
            }

            DWORD Len = 0;
            if (TryReadLengthAt(reinterpret_cast<const BYTE*>(Arg), 0, &Len))
            {
                return Len;
            }

            return 0;
        }

        void RecordInspectDiag(const char* Tag, PVOID Arg0, PVOID Arg1, PVOID Arg2, DWORD LenHint)
        {
            if (Tag == nullptr)
            {
                return;
            }

            LONG Seq = InterlockedIncrement(&g_DiagSeq);
            InspectDiagSlot* Slot = &g_DiagSlots[Seq % kDiagSlots];
            if (InterlockedCompareExchange(&Slot->Ready, -1, 0) != 0)
            {
                return;
            }

            _snprintf_s(Slot->Tag, _TRUNCATE, "%s", Tag);
            Slot->Arg0 = Arg0;
            Slot->Arg1 = Arg1;
            Slot->Arg2 = Arg2;
            Slot->LenHint = LenHint;
            memset(Slot->Preview, 0, sizeof(Slot->Preview));

            const BYTE* PreviewBase = nullptr;
            if (Arg0 != nullptr)
            {
                PreviewBase = reinterpret_cast<const BYTE*>(Arg0);
            }
            else if (Arg1 != nullptr)
            {
                PreviewBase = reinterpret_cast<const BYTE*>(Arg1);
            }

            if (PreviewBase != nullptr)
            {
                DWORD Copied = 0;
                SafeCopyToBuffer(Slot->Preview, PreviewBase, static_cast<DWORD>(sizeof(Slot->Preview)), &Copied);
                (void)Copied;
            }

            InterlockedExchange(&Slot->Ready, 1);
            if (g_DumpWakeEvent != nullptr)
            {
                SetEvent(g_DumpWakeEvent);
            }
        }

        void InspectNLoadImage(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3)
        {
            InterlockedIncrement(&g_HitNLoadImage);

            DWORD Len01 = LengthHintFromArg(Arg1);
            DWORD Len02 = LengthHintFromArg(Arg2);
            RecordInspectDiag("nLoadImage", Arg0, Arg1, Arg2, Len01 != 0 ? Len01 : Len02);

            if (Arg0 != nullptr && Len01 > 0)
            {
                TryDumpRawPE(Arg0, Len01, "nLoadImage_raw01");
            }

            if (Arg1 != nullptr && Len02 > 0 && Arg1 != Arg0)
            {
                TryDumpRawPE(Arg1, Len02, "nLoadImage_raw12");
            }

            TryDumpFromManagedArg(Arg0, "nLoadImage");
            TryDumpFromManagedArg(Arg1, "nLoadImage_arg1");
            (void)Arg3;
        }

        void TryDumpRawOrManaged(PVOID Base, DWORD SizeHint, const char* Tag)
        {
            if (Base == nullptr)
            {
                return;
            }

            PeInfo Direct = ValidatePE(reinterpret_cast<const BYTE*>(Base));
            if (Direct.Base != nullptr && SizeHint > 0)
            {
                TryDumpRawPE(Base, SizeHint, Tag);
                return;
            }

            ResolvedBuffer Resolved = ResolveManagedBuffer(Base, 0);
            if (Resolved.Found && Resolved.Data != nullptr && Resolved.Length > 0)
            {
                TryDumpRawPE(const_cast<BYTE*>(Resolved.Data), Resolved.Length, Tag);
                return;
            }

            if (SizeHint > 0)
            {
                TryDumpRawPE(Base, SizeHint, Tag);
            }
            else
            {
                TryDumpFromManagedArg(Base, Tag);
            }
        }

        void FlushPendingDumps()
        {
            FlushInspectQueue();
            FlushDiagLogs();
            for (int i = 0; i < kDumpQueueSlots; ++i)
            {
                QueuedDump* Slot = &g_DumpQueue[i];
                if (InterlockedCompareExchange(&Slot->Ready, 2, 1) != 1)
                {
                    continue;
                }

                WriteDump(Slot->Data, Slot->Size, Slot->Tag);
                InterlockedExchange(&Slot->Ready, 0);
            }
        }

        DWORD WINAPI DumpWorkerMain(LPVOID)
        {
            for (;;)
            {
                WaitForSingleObject(g_DumpWakeEvent, INFINITE);

                FlushInspectQueue();
                FlushDiagLogs();
                for (int i = 0; i < kDumpQueueSlots; ++i)
                {
                    QueuedDump* Slot = &g_DumpQueue[i];
                    if (InterlockedCompareExchange(&Slot->Ready, 2, 1) != 1)
                    {
                        continue;
                    }

                    WriteDump(Slot->Data, Slot->Size, Slot->Tag);
                    InterlockedExchange(&Slot->Ready, 0);
                }
            }
        }
    } // namespace Detail

    BOOL TryReadLengthAt(const BYTE* Obj, DWORD Offset, DWORD* OutLen)
    {
        if (Obj == nullptr || OutLen == nullptr)
        {
            return FALSE;
        }

        __try
        {
            DWORD Len = *reinterpret_cast<const DWORD*>(Obj + Offset);
            if (Len == 0 || Len > kMaxPeSize)
            {
                return FALSE;
            }
            *OutLen = Len;
            return TRUE;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
    }

    void FlushDiagLogs()
    {
        for (int i = 0; i < kDiagSlots; ++i)
        {
            InspectDiagSlot* Slot = &g_DiagSlots[i];
            if (InterlockedCompareExchange(&Slot->Ready, 2, 1) != 1)
            {
                continue;
            }

            Nemesis_LOG(
                "[?] peek %s arg0=0x%p arg1=0x%p arg2=0x%p len=%lu bytes=%02X%02X%02X%02X%02X%02X%02X%02X",
                Slot->Tag,
                Slot->Arg0,
                Slot->Arg1,
                Slot->Arg2,
                Slot->LenHint,
                Slot->Preview[0], Slot->Preview[1], Slot->Preview[2], Slot->Preview[3],
                Slot->Preview[4], Slot->Preview[5], Slot->Preview[6], Slot->Preview[7]);

            InterlockedExchange(&Slot->Ready, 0);
        }
    }

    void WriteDump(const BYTE* Data, DWORD Size, const char* Tag)
    {
        char DumpDir[MAX_PATH];
        if (!Nemesis::GetDumpDirectory(DumpDir, sizeof(DumpDir)))
        {
            Nemesis_LOG("[-] couldnt get dump folder");
            return;
        }

        CreateDirectoryA(DumpDir, nullptr);

        BYTE* Buffer = static_cast<BYTE*>(malloc(Size));
        if (Buffer == nullptr)
        {
            Nemesis_LOG("[-] dump alloc failed for %lu bytes", Size);
            return;
        }

        DWORD Copied = 0;
        Detail::SafeCopyToBuffer(Buffer, Data, Size, &Copied);
        if (Copied == 0)
        {
            Nemesis_LOG("[-] dump source unreadable @0x%p", Data);
            free(Buffer);
            return;
        }

        SYSTEMTIME St;
        GetLocalTime(&St);
        LONG N = InterlockedIncrement(&g_DumpCounter);

        char Path[MAX_PATH];
        _snprintf_s(Path, MAX_PATH, _TRUNCATE,
                    "%s\\%s_%04u%02u%02u_%02u%02u%02u_%03u_%04ld.bin",
                    DumpDir, Tag,
                    St.wYear, St.wMonth, St.wDay,
                    St.wHour, St.wMinute, St.wSecond, St.wMilliseconds,
                    N);

        HANDLE H = CreateFileA(Path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (H == INVALID_HANDLE_VALUE)
        {
            Nemesis_LOG("[-] dump createfile failed %s err=%lu", Path, GetLastError());
            free(Buffer);
            return;
        }

        DWORD Written = 0;
        BOOL Ok = WriteFile(H, Buffer, Copied, &Written, nullptr);
        CloseHandle(H);
        free(Buffer);

        if (Ok)
        {
            Nemesis_LOG("[+] dumped %s to %s (%lu bytes)", Tag, Path, Written);
        }
        else
        {
            Nemesis_LOG("[-] dump write failed %s err=%lu", Path, GetLastError());
        }
    }

    void TryDumpPE(PVOID Base, const char* Tag)
    {
        if (Base == nullptr)
        {
            return;
        }

        PeInfo Info = Detail::ScanForPE(Base);
        if (Info.Base == nullptr)
        {
            return;
        }

        Detail::QueueDumpSized(Info.Base, Info.FileSize, Tag);
    }

    void TryDumpRawPE(PVOID Base, DWORD SizeHint, const char* Tag)
    {
        if (Base == nullptr)
        {
            return;
        }

        const BYTE* Bytes = reinterpret_cast<const BYTE*>(Base);

        if (SizeHint == 0 || SizeHint > kMaxPeSize)
        {
            TryDumpPE(Base, Tag);
            return;
        }

        PeInfo Direct = Detail::ValidatePE(Bytes);
        if (Direct.Base != nullptr)
        {
            DWORD DumpSize = Direct.FileSize > SizeHint ? SizeHint : Direct.FileSize;
            Detail::QueueDumpSized(Direct.Base, DumpSize, Tag);
            return;
        }

        if (Detail::HasMzMagic(Bytes))
        {
            Detail::QueueDumpSized(Bytes, SizeHint, Tag);
            return;
        }

        ResolvedBuffer Resolved = Detail::ResolveManagedBuffer(Base, 0);
        if (Resolved.Found && Resolved.Data != nullptr && Resolved.Length > 0)
        {
            Detail::QueueDumpSized(Resolved.Data, Resolved.Length, Tag);
            return;
        }

        if (SizeHint > 0 && SizeHint <= kMaxPeSize)
        {
            DWORD Copied = 0;
            BYTE Probe[2] = {};
            Detail::SafeCopyToBuffer(Probe, Bytes, sizeof(Probe), &Copied);
            if (Copied > 0)
            {
                Detail::QueueDumpSized(Bytes, SizeHint, Tag);
            }
            return;
        }

        TryDumpPE(Base, Tag);
    }

    void TryDumpFromManagedArg(PVOID Arg, const char* Tag)
    {
        ResolvedBuffer Resolved = Detail::ResolveManagedBuffer(Arg, 0);
        if (Resolved.Found && Resolved.Data != nullptr)
        {
            if (Resolved.Length > 0)
            {
                TryDumpRawPE(const_cast<BYTE*>(Resolved.Data), Resolved.Length, Tag);
            }
            else
            {
                TryDumpPE(const_cast<BYTE*>(Resolved.Data), Tag);
            }
            return;
        }

        TryDumpPE(Arg, Tag);
    }

}

extern "C" void ClrNativeInspect_FlushPendingDumps()
{
    using namespace Nemesis::Hooks::ClrNativeInspect;

    Nemesis_LOG("[?] hook hits nLoadImage=%ld nLoadFile=%ld LoadFromBuffer=%ld",g_HitNLoadImage,g_HitNLoadFile,g_HitLoadFromBuffer);
    Detail::FlushPendingDumps();
}

extern "C" void ClrNativeInspect_StartDumpWorker()
{
    using namespace Nemesis::Hooks::ClrNativeInspect;

    if (Detail::g_DumpWorkerThread != nullptr)
    {
        return;
    }

    g_DumpWakeEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (g_DumpWakeEvent == nullptr)
    {
        return;
    }

    Detail::g_DumpWorkerThread = CreateThread(nullptr, 0, Detail::DumpWorkerMain, nullptr, 0, nullptr);
}

extern "C" VOID Nemesis_CLR_INSPECT_CC
ClrNative_NLoadImage_Inspect(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3)
{
    using namespace Nemesis::Hooks::ClrNativeInspect;

    Detail::QueueInspectWork("nLoadImage", Arg0, Arg1, Arg2, Arg3, 0);
}

extern "C" VOID Nemesis_CLR_INSPECT_CC
ClrNative_NLoadFile_Inspect(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3)
{
    (void)Arg0;
    (void)Arg1;
    (void)Arg2;
    (void)Arg3;
}

extern "C" VOID Nemesis_CLR_INSPECT_CC
ClrNative_LoadFromBuffer_Inspect(
    PVOID Arg0,
    PVOID RawAssemblyBuffer,
    INT32 RawAssemblyLength,
    PVOID Arg3,
    PVOID CallerReturnAddress)
{
    using namespace Nemesis::Hooks::ClrNativeInspect;

    (void)CallerReturnAddress;

    INT32 LenHint = RawAssemblyLength;
    if (LenHint <= 0 && Arg3 != nullptr)
    {
        ULONG_PTR Raw = reinterpret_cast<ULONG_PTR>(Arg3);
        if (Raw >= 0x1000 && Raw <= kMaxPeSize)
        {
            LenHint = static_cast<INT32>(Raw);
        }
    }

    Detail::QueueInspectWork(
        "LoadFromBuffer",
        Arg0,
        RawAssemblyBuffer,
        Arg3,
        nullptr,
        LenHint);
}

extern "C" VOID Nemesis_CLR_INSPECT_CC
ClrJit_CompileMethod_Inspect(PVOID Arg0, PVOID Arg1, PVOID Arg2, PVOID Arg3)
{
    (void)Arg0;
    (void)Arg1;
    (void)Arg2;
    (void)Arg3;
}
