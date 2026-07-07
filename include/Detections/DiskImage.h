#pragma once

#include "Common.h"

namespace Nemesis::Detections::DiskImage
{
    struct MappedImage
    {
        HANDLE FileHandle;
        HANDLE MapHandle;
        BYTE*  Base;
        DWORD  ImageSize;
    };

    bool   Load(const char* ModuleName, MappedImage& Out);
    void   Free(MappedImage& Image);

    bool   GetSection(BYTE* ModuleBase, const char* SectionName, BYTE** OutBase, DWORD* OutRva, DWORD* OutSize);
    LPVOID FindByteSequence(BYTE* Base, DWORD Size, const BYTE* Pattern, DWORD PatternLen);
    DWORD  GetImageSize(BYTE* ModuleBase);
}
