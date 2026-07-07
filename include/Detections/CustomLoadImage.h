#pragma once

#include "Common.h"

namespace Nemesis::Detections::CustomLoadImage
{
    void   SetNLoadImageBase(LPVOID Base);
    LPVOID ResolveLoadFromBuffer();
}
