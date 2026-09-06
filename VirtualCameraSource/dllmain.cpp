#include <windows.h>
#include <cstdio>

HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hModule;

        DisableThreadLibraryCalls(hModule);

        OutputDebugStringW(
            L"[VirtualCameraSource] DLL_PROCESS_ATTACH\n"
        );
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] DLL_PROCESS_DETACH\n"
        );
    }

    return TRUE;
}