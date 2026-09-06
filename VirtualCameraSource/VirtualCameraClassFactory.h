#pragma once

#include <unknwn.h>

class VirtualCameraClassFactory final : public IClassFactory
{
public:
    VirtualCameraClassFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(
        REFIID riid,
        void** ppv) override;

    STDMETHODIMP_(ULONG) AddRef() override;

    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(
        IUnknown* outer,
        REFIID riid,
        void** ppv) override;

    STDMETHODIMP LockServer(
        BOOL lock) override;

private:
    ~VirtualCameraClassFactory();

    ULONG m_refCount = 1;
};