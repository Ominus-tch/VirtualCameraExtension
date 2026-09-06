#pragma once

#include <guiddef.h>
#include <Windows.h>

DEFINE_GUID(
    CLSID_PianoVisualizerVirtualCameraSource,
    0x8f9c6c1a, 0x4e2d, 0x4b3a,
    0x91, 0x7f, 0x32, 0x5c, 0x7a, 0x4e, 0x91, 0x20
);

// Activation attributes used by the virtual-camera media source.

DEFINE_GUID(
    VCAM_KIND,
    0xc7f7c57b, 0xdf30, 0x41d0,
    0xaf, 0xfc, 0x15, 0x20, 0x1c, 0xdf, 0x92, 0x0d
);

enum class VirtualCameraKind : UINT32
{
    Synthetic = 0,
    BasicCameraWrapper = 1,
    AugmentedCameraWrapper = 2
};