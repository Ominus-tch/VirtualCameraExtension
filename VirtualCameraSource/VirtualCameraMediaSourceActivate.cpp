#include "VirtualCameraMediaSourceActivate.h"

#include "VirtualCameraGuids.h"
#include "VirtualCameraExtension.h"

#include <mfvirtualcamera.h>

#include <cwchar>

#define FORWARD_ATTRIBUTE_METHOD(method, args) \
    if (!m_attributes) \
        return MF_E_NOT_INITIALIZED; \
    return m_attributes->method args;

VirtualCameraMediaSourceActivate::VirtualCameraMediaSourceActivate()
{}

VirtualCameraMediaSourceActivate::~VirtualCameraMediaSourceActivate()
{
    /*
        IMPORTANT:

        Do NOT shut down the media source here.

        Frame Server can release the activation object while
        it still owns and uses the media source.
    */
    if (m_mediaSource)
    {
        m_mediaSource->Release();
        m_mediaSource = nullptr;
    }

    if (m_attributes)
    {
        m_attributes->Release();
        m_attributes = nullptr;
    }
}

STDMETHODIMP VirtualCameraMediaSourceActivate::QueryInterface(
    REFIID riid,
    void** ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    wchar_t iidString[64]{};

    StringFromGUID2(
        riid,
        iidString,
        ARRAYSIZE(iidString)
    );

    wchar_t message[256]{};

    swprintf_s(
        message,
        L"[VirtualCameraActivate] QueryInterface: %s\n",
        iidString
    );

    OutputDebugStringW(message);

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(
            static_cast<IMFActivate*>(this)
            );
    }
    else if (riid == IID_IMFActivate)
    {
        *ppv = static_cast<IMFActivate*>(this);
    }
    else if (riid == IID_IMFAttributes)
    {
        *ppv = static_cast<IMFAttributes*>(
            static_cast<IMFActivate*>(this)
            );
    }
    else
    {
        OutputDebugStringW(
            L"[VirtualCameraActivate] QueryInterface FAILED\n"
        );

        return E_NOINTERFACE;
    }

    AddRef();

    OutputDebugStringW(
        L"[VirtualCameraActivate] QueryInterface SUCCEEDED\n"
    );

    return S_OK;
}

STDMETHODIMP_(ULONG)
VirtualCameraMediaSourceActivate::AddRef()
{
    return ++m_refCount;
}

STDMETHODIMP_(ULONG)
VirtualCameraMediaSourceActivate::Release()
{
    ULONG count = --m_refCount;

    if (count == 0)
        delete this;

    return count;
}

HRESULT VirtualCameraMediaSourceActivate::Initialize(
    const VirtualCameraFormat& format)
{
    if (m_attributes)
        return S_OK;

    m_format =
        format.IsValid()
        ? format
        : VirtualCameraFormat{};

    HRESULT hr = MFCreateAttributes(
        &m_attributes,
        2
    );

    if (FAILED(hr))
        return hr;

    hr = m_attributes->SetUINT32(
        MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES,
        1
    );

    if (FAILED(hr))
        return hr;

    /*
        Identify this activation object as our synthetic
        virtual-camera source.
    */
    hr = m_attributes->SetUINT32(
        VCAM_KIND,
        static_cast<UINT32>(
            VirtualCameraKind::Synthetic
            )
    );

    return hr;
}

STDMETHODIMP VirtualCameraMediaSourceActivate::ActivateObject(
    REFIID riid,
    void** ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    OutputDebugStringW(
        L"[VirtualCameraActivate] ActivateObject()\n"
    );

    if (!m_attributes)
    {
        OutputDebugStringW(
            L"[VirtualCameraActivate] "
            L"Attributes not initialized\n"
        );

        return MF_E_NOT_INITIALIZED;
    }

    UINT32 kind = 0;

    HRESULT hr = m_attributes->GetUINT32(
        VCAM_KIND,
        &kind
    );

    if (FAILED(hr))
        return hr;

    if (kind != static_cast<UINT32>(
        VirtualCameraKind::Synthetic))
    {
        OutputDebugStringW(
            L"[VirtualCameraActivate] "
            L"Unsupported virtual camera kind\n"
        );

        return E_NOTIMPL;
    }

    if (!m_mediaSource)
    {
        auto* source =
            new VirtualCameraSource(
                m_attributes,
                m_format
            );

        if (!source)
            return E_OUTOFMEMORY;

        m_mediaSource = source;

        OutputDebugStringW(
            L"[VirtualCameraActivate] "
            L"VirtualCameraSource created\n"
        );
    }

    return m_mediaSource->QueryInterface(
        riid,
        ppv
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::ShutdownObject()
{
    OutputDebugStringW(
        L"[VirtualCameraActivate] !!! ShutdownObject() CALLED !!!\n"
    );

    if (m_mediaSource)
    {
        OutputDebugStringW(
            L"[VirtualCameraActivate] Calling mediaSource->Shutdown()\n"
        );

        HRESULT hr =
            m_mediaSource->Shutdown();

        wchar_t message[128]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraActivate] mediaSource->Shutdown() -> 0x%08X\n",
            static_cast<unsigned int>(hr)
        );

        OutputDebugStringW(message);
    }

    return S_OK;
}

STDMETHODIMP VirtualCameraMediaSourceActivate::DetachObject()
{
    OutputDebugStringW(
        L"[VirtualCameraActivate] DetachObject()\n"
    );

    if (m_mediaSource)
    {
        m_mediaSource->Release();
        m_mediaSource = nullptr;
    }

    return S_OK;
}


// ============================================================
// IMFAttributes delegation
// ============================================================

STDMETHODIMP VirtualCameraMediaSourceActivate::GetItem(
    REFGUID guidKey,
    PROPVARIANT* value)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetItem,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetItemType(
    REFGUID guidKey,
    MF_ATTRIBUTE_TYPE* type)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetItemType,
        (guidKey, type)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::CompareItem(
    REFGUID guidKey,
    REFPROPVARIANT value,
    BOOL* result)
{
    FORWARD_ATTRIBUTE_METHOD(
        CompareItem,
        (guidKey, value, result)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::Compare(
    IMFAttributes* theirs,
    MF_ATTRIBUTES_MATCH_TYPE matchType,
    BOOL* result)
{
    FORWARD_ATTRIBUTE_METHOD(
        Compare,
        (theirs, matchType, result)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetUINT32(
    REFGUID guidKey,
    UINT32* value)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetUINT32,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetUINT64(
    REFGUID guidKey,
    UINT64* value)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetUINT64,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetDouble(
    REFGUID guidKey,
    double* value)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetDouble,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetGUID(
    REFGUID guidKey,
    GUID* value)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetGUID,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetStringLength(
    REFGUID guidKey,
    UINT32* length)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetStringLength,
        (guidKey, length)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetString(
    REFGUID guidKey,
    LPWSTR value,
    UINT32 bufferSize,
    UINT32* length)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetString,
        (guidKey, value, bufferSize, length)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetAllocatedString(
    REFGUID guidKey,
    LPWSTR* value,
    UINT32* length)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetAllocatedString,
        (guidKey, value, length)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetBlobSize(
    REFGUID guidKey,
    UINT32* size)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetBlobSize,
        (guidKey, size)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetBlob(
    REFGUID guidKey,
    UINT8* buffer,
    UINT32 bufferSize,
    UINT32* size)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetBlob,
        (guidKey, buffer, bufferSize, size)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetAllocatedBlob(
    REFGUID guidKey,
    UINT8** buffer,
    UINT32* size)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetAllocatedBlob,
        (guidKey, buffer, size)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetUnknown(
    REFGUID guidKey,
    REFIID riid,
    LPVOID* ppv)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetUnknown,
        (guidKey, riid, ppv)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetItem(
    REFGUID guidKey,
    REFPROPVARIANT value)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetItem,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::DeleteItem(
    REFGUID guidKey)
{
    FORWARD_ATTRIBUTE_METHOD(
        DeleteItem,
        (guidKey)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::DeleteAllItems()
{
    FORWARD_ATTRIBUTE_METHOD(
        DeleteAllItems,
        ()
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetUINT32(
    REFGUID guidKey,
    UINT32 value)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetUINT32,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetUINT64(
    REFGUID guidKey,
    UINT64 value)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetUINT64,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetDouble(
    REFGUID guidKey,
    double value)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetDouble,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetGUID(
    REFGUID guidKey,
    REFGUID value)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetGUID,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetString(
    REFGUID guidKey,
    LPCWSTR value)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetString,
        (guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetBlob(
    REFGUID guidKey,
    const UINT8* buffer,
    UINT32 bufferSize)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetBlob,
        (guidKey, buffer, bufferSize)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::SetUnknown(
    REFGUID guidKey,
    IUnknown* unknown)
{
    FORWARD_ATTRIBUTE_METHOD(
        SetUnknown,
        (guidKey, unknown)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::LockStore()
{
    FORWARD_ATTRIBUTE_METHOD(
        LockStore,
        ()
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::UnlockStore()
{
    FORWARD_ATTRIBUTE_METHOD(
        UnlockStore,
        ()
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetCount(
    UINT32* count)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetCount,
        (count)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::GetItemByIndex(
    UINT32 index,
    GUID* guidKey,
    PROPVARIANT* value)
{
    FORWARD_ATTRIBUTE_METHOD(
        GetItemByIndex,
        (index, guidKey, value)
    );
}

STDMETHODIMP VirtualCameraMediaSourceActivate::CopyAllItems(
    IMFAttributes* destination)
{
    FORWARD_ATTRIBUTE_METHOD(
        CopyAllItems,
        (destination)
    );
}

#undef FORWARD_ATTRIBUTE_METHOD