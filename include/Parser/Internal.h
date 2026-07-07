#pragma once

#include "Common.h"

namespace Nemesis::Parser::Internal
{
    LPVOID ScanMemoryForPattern(LPVOID StartAddr, DWORD SearchSize, BYTE* Pattern, DWORD PatternLen);
    LPVOID ResolveInternalFunction(const char* DllName, const char* FuncName);
}
