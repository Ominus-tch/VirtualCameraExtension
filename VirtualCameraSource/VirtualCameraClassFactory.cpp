#include "VirtualCameraClassFactory.h"
#include "VirtualCameraMediaSourceActivate.h"

VirtualCameraClassFactory::VirtualCameraClassFactory()
{}

VirtualCameraClassFactory::~VirtualCameraClassFactory()
{}

STDMETHODIMP VirtualCameraClassFactory::QueryInterface(
    REFIID riid,
    void** ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    if (riid == IID_IUnknown ||
        riid == IID_IClassFactory)
    {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
VirtualCameraClassFactory::AddRef()
{
    return ++m_refCount;
}

STDMETHODIMP_(ULONG)
VirtualCameraClassFactory::Release()
{
    ULONG count = --m_refCount;

    if (count == 0)
        delete this;

    return count;
}

STDMETHODIMP VirtualCameraClassFactory::CreateInstance(
    IUnknown* outer,
    REFIID riid,
    void** ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    if (outer)
        return CLASS_E_NOAGGREGATION;

    auto* activate =
        new VirtualCameraMediaSourceActivate();

    if (!activate)
        return E_OUTOFMEMORY;

    HRESULT hr = activate->Initialize();

    if (SUCCEEDED(hr))
    {
        hr = activate->QueryInterface(
            riid,
            ppv
        );
    }

    activate->Release();

    return hr;
}

STDMETHODIMP VirtualCameraClassFactory::LockServer(
    BOOL)
{
    return S_OK;
}