#pragma once

#include <windows.h>

struct VirtualCameraFormat
{
    UINT32 width = 1920;
    UINT32 height = 1080;

    UINT32 frameRateNumerator = 30;
    UINT32 frameRateDenominator = 1;

    bool IsValid() const
    {
        return width > 0 &&
            height > 0 &&
            frameRateNumerator > 0 &&
            frameRateDenominator > 0;
    }
};