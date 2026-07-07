#pragma once

#include <windows.h>

namespace Nemesis::Init
{
    void OnAttach(HMODULE hModule);
    void OnDetach();
}
