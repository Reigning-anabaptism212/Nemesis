#include "Detections/DiskImage.h"

#include <cstring>
// dont be dumb like me and dont comprae mem vs mem xD
namespace Nemesis::Detections::DiskImage
{
    bool Load(const char* ModuleName, MappedImage& Out)
    {
        Out.FileHandle = INVALID_HANDLE_VALUE;
        Out.MapHandle  = nullptr;
        Out.Base       = nullptr;
        Out.ImageSize  = 0;

        HMODULE MemMod = GetModuleHandleA(ModuleName);
        if (MemMod == nullptr)
        {
            return false;
        }

        char Path[MAX_PATH] = {};
        if (GetModuleFileNameA(MemMod, Path, MAX_PATH) == 0)
        {
            return false;
        }

        HANDLE File = CreateFileA(
            Path,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (File == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        HANDLE Map = CreateFileMappingA(
            File,
            nullptr,
            PAGE_READONLY | SEC_IMAGE,
            0,
            0,
            nullptr
        );
        if (Map == nullptr)
        {
            CloseHandle(File);
            return false;
        }

        BYTE* Base = reinterpret_cast<BYTE*>(MapViewOfFile(Map, FILE_MAP_READ, 0, 0, 0));
        if (Base == nullptr)
        {
            CloseHandle(Map);
            CloseHandle(File);
            return false;
        }

        Out.FileHandle = File;
        Out.MapHandle  = Map;
        Out.Base       = Base;
        Out.ImageSize  = GetImageSize(Base);
        return Out.ImageSize != 0;
    }

    void Free(MappedImage& Image)
    {
        if (Image.Base != nullptr)
        {
            UnmapViewOfFile(Image.Base);
        }
        if (Image.MapHandle != nullptr)
        {
            CloseHandle(Image.MapHandle);
        }
        if (Image.FileHandle != INVALID_HANDLE_VALUE && Image.FileHandle != nullptr)
        {
            CloseHandle(Image.FileHandle);
        }

        Image.FileHandle = INVALID_HANDLE_VALUE;
        Image.MapHandle  = nullptr;
        Image.Base       = nullptr;
        Image.ImageSize  = 0;
    }

    DWORD GetImageSize(BYTE* ModuleBase)
    {
        if (ModuleBase == nullptr)
        {
            return 0;
        }

        __try
        {
            IMAGE_DOS_HEADER* Dos = reinterpret_cast<IMAGE_DOS_HEADER*>(ModuleBase);
            if (Dos->e_magic != IMAGE_DOS_SIGNATURE)
            {
                return 0;
            }
            IMAGE_NT_HEADERS* Nt = reinterpret_cast<IMAGE_NT_HEADERS*>(ModuleBase + Dos->e_lfanew);
            if (Nt->Signature != IMAGE_NT_SIGNATURE)
            {
                return 0;
            }
            return Nt->OptionalHeader.SizeOfImage;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }

    bool GetSection(BYTE* ModuleBase, const char* Name, BYTE** OutBase, DWORD* OutRva, DWORD* OutSize)
    {
        if (ModuleBase == nullptr || Name == nullptr)
        {
            return false;
        }

        __try
        {
            IMAGE_DOS_HEADER* Dos = reinterpret_cast<IMAGE_DOS_HEADER*>(ModuleBase);
            if (Dos->e_magic != IMAGE_DOS_SIGNATURE)
            {
                return false;
            }
            IMAGE_NT_HEADERS* Nt = reinterpret_cast<IMAGE_NT_HEADERS*>(ModuleBase + Dos->e_lfanew);
            if (Nt->Signature != IMAGE_NT_SIGNATURE)
            {
                return false;
            }

            IMAGE_SECTION_HEADER* Sec = IMAGE_FIRST_SECTION(Nt);
            WORD Count = Nt->FileHeader.NumberOfSections;

            for (WORD i = 0; i < Count; ++i)
            {
                char SecName[9] = {};
                memcpy(SecName, Sec[i].Name, 8);
                if (strncmp(SecName, Name, 8) == 0)
                {
                    if (OutBase != nullptr)
                    {
                        *OutBase = ModuleBase + Sec[i].VirtualAddress;
                    }
                    if (OutRva != nullptr)
                    {
                        *OutRva = Sec[i].VirtualAddress;
                    }
                    if (OutSize != nullptr)
                    {
                        DWORD VSize = Sec[i].Misc.VirtualSize;
                        DWORD RSize = Sec[i].SizeOfRawData;
                        *OutSize = (VSize < RSize) ? VSize : RSize;
                    }
                    return true;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        return false;
    }

    LPVOID FindByteSequence(BYTE* Base, DWORD Size, const BYTE* Pattern, DWORD PatternLen)
    {
        if (Base == nullptr || Pattern == nullptr || PatternLen == 0 || Size < PatternLen)
        {
            return nullptr;
        }

        DWORD Limit = Size - PatternLen;
        for (DWORD i = 0; i <= Limit; ++i)
        {
            bool Match = true;
            for (DWORD j = 0; j < PatternLen; ++j)
            {
                if (Base[i + j] != Pattern[j])
                {
                    Match = false;
                    break;
                }
            }
            if (Match)
            {
                return Base + i;
            }
        }
        return nullptr;
    }
}
