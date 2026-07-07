#pragma once

#include <windows.h>
#include <cctype>
#include <cstring>

namespace Nemesis::Util
{
    inline bool EndsWithI(const char* Path, const char* Suffix)
    {
        if (Path == nullptr || Suffix == nullptr)
        {
            return false;
        }

        size_t PathLen = strlen(Path);
        size_t SuffixLen = strlen(Suffix);
        if (PathLen < SuffixLen)
        {
            return false;
        }

        const char* Tail = Path + (PathLen - SuffixLen);
        for (size_t i = 0; i < SuffixLen; ++i)
        {
            if (tolower(static_cast<unsigned char>(Tail[i])) != tolower(static_cast<unsigned char>(Suffix[i])))
            {
                return false;
            }
        }

        return true;
    }

    inline bool GetSelfDir(char* Out, size_t OutSize)
    {
        if (Out == nullptr || OutSize == 0)
        {
            return false;
        }

        if (GetModuleFileNameA(nullptr, Out, static_cast<DWORD>(OutSize)) == 0)
        {
            Out[0] = '\0';
            return false;
        }

        for (size_t i = strlen(Out); i > 0; --i)
        {
            if (Out[i - 1] == '\\' || Out[i - 1] == '/')
            {
                Out[i - 1] = '\0';
                break;
            }
        }

        return Out[0] != '\0';
    }
}
