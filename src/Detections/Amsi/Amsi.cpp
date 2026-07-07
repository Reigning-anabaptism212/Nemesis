#include "Detections/Amsi/Amsi.h"
#include "Detections/DiskImage.h"
#include "Detections/ExportCompare.h"

#include <cstring>

namespace Nemesis::Detections::Amsi
{
    namespace
    {
        void CheckClrStringBypass()
        {
            HMODULE ClrMem = GetModuleHandleA("clr.dll");
            if (ClrMem == nullptr)
            {
                Nemesis_LOG("[?] amsi clr not loaded yet skipping rdata string check");
                return;
            }

            BYTE* MemBase = reinterpret_cast<BYTE*>(ClrMem);

            DiskImage::MappedImage Disk = {};
            if (!DiskImage::Load("clr.dll", Disk))
            {
                Nemesis_LOG("[?] amsi cant map clr.dll from disk for rdata check");
                return;
            }

            BYTE* RdataBase = nullptr;
            DWORD RdataSize = 0;
            if (!DiskImage::GetSection(Disk.Base, ".rdata", &RdataBase, nullptr, &RdataSize))
            {
                Nemesis_LOG("[?] amsi clr .rdata not found on disk");
                DiskImage::Free(Disk);
                return;
            }

            for (const char* S : kClrRdataStrings)
            {
                DWORD Len = static_cast<DWORD>(strlen(S) + 1);
                LPVOID DiskHit = DiskImage::FindByteSequence(RdataBase, RdataSize, reinterpret_cast<const BYTE*>(S), Len);
                if (DiskHit == nullptr)
                {
                    Nemesis_LOG("[?] amsi clr rdata missing '%s' on disk skipping", S);
                    continue;
                }

                DWORD Rva = static_cast<DWORD>(reinterpret_cast<BYTE*>(DiskHit) - Disk.Base);
                const BYTE* MemPtr = MemBase + Rva;

                __try
                {
                    if (memcmp(MemPtr, S, Len) != 0)
                    {
                        Nemesis_LOG(
                            "[+] amsi tampered clr+0x%lx expected '%s' mem=%02X %02X %02X %02X %02X %02X %02X %02X",
                            Rva,
                            S,
                            MemPtr[0], MemPtr[1], MemPtr[2], MemPtr[3],
                            MemPtr[4], MemPtr[5], MemPtr[6], MemPtr[7]);

                        Nemesis_LOG("[+] amsi string replace bypass on '%s'", S);
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    Nemesis_LOG("[?] amsi fault reading clr+0x%lx for '%s'", Rva, S);
                }
            }

            DiskImage::Free(Disk);
        }
    }

    void CheckAll()
    {
        for (const char* Name : kAmsiExports)
        {
            ExportCompare::CheckExport("amsi", "amsi.dll", Name, ExportCompare::LoadPolicy::TryLoad);
        }

        CheckClrStringBypass();
    }
}
