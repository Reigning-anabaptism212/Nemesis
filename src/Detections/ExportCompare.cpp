#include "Detections/ExportCompare.h"
#include "Detections/DiskImage.h"

namespace Nemesis::Detections::ExportCompare
{
    namespace
    {
        void LogProbeBytes(const char* Tag, const char* Label, const BYTE* Bytes)
        {
            Nemesis_LOG(
                "[+] %s %s %02X %02X %02X %02X %02X %02X %02X %02X",
                Tag,
                Label,
                Bytes[0], Bytes[1], Bytes[2], Bytes[3],
                Bytes[4], Bytes[5], Bytes[6], Bytes[7]);
        }
    }

    void CheckExport(const char* Tag, const char* ModuleName, const char* ExportName, LoadPolicy Policy)
    {
        HMODULE MemMod = GetModuleHandleA(ModuleName);
        if (MemMod == nullptr && Policy == LoadPolicy::TryLoad)
        {
            MemMod = LoadLibraryA(ModuleName);
        }

        if (MemMod == nullptr)
        {
            if (Policy == LoadPolicy::MustBeLoaded)
            {
                Nemesis_LOG("[?] %s %s not loaded skipping %s", Tag, ModuleName, ExportName);
            }
            else
            {
                Nemesis_LOG("[?] %s %s not in this process", Tag, ModuleName);
            }
            return;
        }

        FARPROC MemProc = GetProcAddress(MemMod, ExportName);
        if (MemProc == nullptr)
        {
            Nemesis_LOG("[?] %s %s!%s not exported", Tag, ModuleName, ExportName);
            return;
        }

        BYTE* MemBase = reinterpret_cast<BYTE*>(MemMod);
        DWORD ImageSize = DiskImage::GetImageSize(MemBase);
        if (ImageSize != 0)
        {
            const BYTE* Ptr = reinterpret_cast<const BYTE*>(MemProc);
            if (Ptr < MemBase || Ptr >= (MemBase + ImageSize))
            {
                Nemesis_LOG("[?] %s %s!%s forwarded @0x%p skipping", Tag, ModuleName, ExportName, MemProc);
                return;
            }
        }

        DiskImage::MappedImage Disk = {};
        if (!DiskImage::Load(ModuleName, Disk))
        {
            Nemesis_LOG("[?] %s cant map %s from disk", Tag, ModuleName);
            return;
        }

        DWORD Rva = static_cast<DWORD>(reinterpret_cast<BYTE*>(MemProc) - MemBase);
        const BYTE* MemBytes = reinterpret_cast<const BYTE*>(MemProc);
        const BYTE* DiskBytes = Disk.Base + Rva;

        DWORD DiffCount = 0;
        DWORD FirstDiffAt = 0;

        __try
        {
            for (DWORD i = 0; i < kProbeBytes; ++i)
            {
                if (MemBytes[i] != DiskBytes[i])
                {
                    if (DiffCount == 0)
                    {
                        FirstDiffAt = i;
                    }
                    ++DiffCount;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Nemesis_LOG("[?] %s fault comparing %s!%s", Tag, ModuleName, ExportName);
            DiskImage::Free(Disk);
            return;
        }

        if (DiffCount > 0)
        {
            Nemesis_LOG(
                "[+] %s tampered %s!%s @0x%p rva=0x%lx %lu/%lu bytes differ first +0x%lx disk=%02X mem=%02X",
                Tag,
                ModuleName,
                ExportName,
                MemProc,
                Rva,
                DiffCount,
                kProbeBytes,
                FirstDiffAt,
                DiskBytes[FirstDiffAt],
                MemBytes[FirstDiffAt]);

            LogProbeBytes(Tag, "mem", MemBytes);
            LogProbeBytes(Tag, "disk", DiskBytes);
        }

        DiskImage::Free(Disk);
    }
}
