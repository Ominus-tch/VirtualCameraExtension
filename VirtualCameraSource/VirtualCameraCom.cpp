#include <windows.h>

#include "VirtualCameraClassFactory.h"

#include <initguid.h>

extern HMODULE g_hModule;

#include "VirtualCameraGuids.h"

STDAPI DllGetClassObject(
    REFCLSID rclsid,
    REFIID riid,
    void** ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    if (rclsid !=
        CLSID_PianoVisualizerVirtualCameraSource)
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory =
        new VirtualCameraClassFactory();

    if (!factory)
        return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(
        riid,
        ppv
    );

    factory->Release();

    return hr;
}

STDAPI DllCanUnloadNow()
{
    return S_FALSE;
}

STDAPI DllRegisterServer()
{
    wchar_t modulePath[MAX_PATH];

    DWORD length = GetModuleFileNameW(
        g_hModule,
        modulePath,
        ARRAYSIZE(modulePath)
    );

    if (length == 0 || length >= ARRAYSIZE(modulePath))
        return HRESULT_FROM_WIN32(GetLastError());

    HKEY clsidKey = nullptr;

    LONG result = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Classes\\CLSID\\"
        L"{8F9C6C1A-4E2D-4B3A-917F-325C7A4E9120}",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &clsidKey,
        nullptr
    );

    if (result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(result);

    result = RegSetValueExW(
        clsidKey,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(
            L"Piano Visualizer Virtual Camera Source"
            ),
        sizeof(L"Piano Visualizer Virtual Camera Source")
    );

    if (result != ERROR_SUCCESS)
    {
        RegCloseKey(clsidKey);
        return HRESULT_FROM_WIN32(result);
    }

    HKEY inprocKey = nullptr;

    result = RegCreateKeyExW(
        clsidKey,
        L"InProcServer32",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &inprocKey,
        nullptr
    );

    if (result != ERROR_SUCCESS)
    {
        RegCloseKey(clsidKey);
        return HRESULT_FROM_WIN32(result);
    }

    result = RegSetValueExW(
        inprocKey,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(modulePath),
        static_cast<DWORD>(
            (wcslen(modulePath) + 1) * sizeof(wchar_t)
            )
    );

    if (result == ERROR_SUCCESS)
    {
        result = RegSetValueExW(
            inprocKey,
            L"ThreadingModel",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(L"Both"),
            sizeof(L"Both")
        );
    }

    RegCloseKey(inprocKey);
    RegCloseKey(clsidKey);

    return HRESULT_FROM_WIN32(result);
}

STDAPI DllUnregisterServer()
{
    LONG result = RegDeleteTreeW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Classes\\CLSID\\"
        L"{8F9C6C1A-4E2D-4B3A-917F-325C7A4E9120}"
    );

    if (result == ERROR_FILE_NOT_FOUND)
        return S_OK;

    return HRESULT_FROM_WIN32(result);
}