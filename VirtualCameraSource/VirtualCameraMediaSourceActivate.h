#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

#include <atomic>

#include "VirtualCameraFormat.h"

class VirtualCameraMediaSourceActivate final
    : public IMFActivate
    , public IMFAttributes
{
public:
    VirtualCameraMediaSourceActivate();

    // IUnknown
    STDMETHODIMP QueryInterface(
        REFIID riid,
        void** ppv
    ) override;

    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IMFActivate
    STDMETHODIMP ActivateObject(
        REFIID riid,
        void** ppv
    ) override;

    STDMETHODIMP ShutdownObject() override;

    STDMETHODIMP DetachObject() override;

    // IMFAttributes
    STDMETHODIMP GetItem(
        REFGUID guidKey,
        PROPVARIANT* value
    ) override;

    STDMETHODIMP GetItemType(
        REFGUID guidKey,
        MF_ATTRIBUTE_TYPE* type
    ) override;

    STDMETHODIMP CompareItem(
        REFGUID guidKey,
        REFPROPVARIANT value,
        BOOL* result
    ) override;

    STDMETHODIMP Compare(
        IMFAttributes* theirs,
        MF_ATTRIBUTES_MATCH_TYPE matchType,
        BOOL* result
    ) override;

    STDMETHODIMP GetUINT32(
        REFGUID guidKey,
        UINT32* value
    ) override;

    STDMETHODIMP GetUINT64(
        REFGUID guidKey,
        UINT64* value
    ) override;

    STDMETHODIMP GetDouble(
        REFGUID guidKey,
        double* value
    ) override;

    STDMETHODIMP GetGUID(
        REFGUID guidKey,
        GUID* value
    ) override;

    STDMETHODIMP GetStringLength(
        REFGUID guidKey,
        UINT32* length
    ) override;

    STDMETHODIMP GetString(
        REFGUID guidKey,
        LPWSTR value,
        UINT32 bufferSize,
        UINT32* length
    ) override;

    STDMETHODIMP GetAllocatedString(
        REFGUID guidKey,
        LPWSTR* value,
        UINT32* length
    ) override;

    STDMETHODIMP GetBlobSize(
        REFGUID guidKey,
        UINT32* size
    ) override;

    STDMETHODIMP GetBlob(
        REFGUID guidKey,
        UINT8* buffer,
        UINT32 bufferSize,
        UINT32* size
    ) override;

    STDMETHODIMP GetAllocatedBlob(
        REFGUID guidKey,
        UINT8** buffer,
        UINT32* size
    ) override;

    STDMETHODIMP GetUnknown(
        REFGUID guidKey,
        REFIID riid,
        LPVOID* ppv
    ) override;

    STDMETHODIMP SetItem(
        REFGUID guidKey,
        REFPROPVARIANT value
    ) override;

    STDMETHODIMP DeleteItem(
        REFGUID guidKey
    ) override;

    STDMETHODIMP DeleteAllItems() override;

    STDMETHODIMP SetUINT32(
        REFGUID guidKey,
        UINT32 value
    ) override;

    STDMETHODIMP SetUINT64(
        REFGUID guidKey,
        UINT64 value
    ) override;

    STDMETHODIMP SetDouble(
        REFGUID guidKey,
        double value
    ) override;

    STDMETHODIMP SetGUID(
        REFGUID guidKey,
        REFGUID value
    ) override;

    STDMETHODIMP SetString(
        REFGUID guidKey,
        LPCWSTR value
    ) override;

    STDMETHODIMP SetBlob(
        REFGUID guidKey,
        const UINT8* buffer,
        UINT32 bufferSize
    ) override;

    STDMETHODIMP SetUnknown(
        REFGUID guidKey,
        IUnknown* unknown
    ) override;

    STDMETHODIMP LockStore() override;

    STDMETHODIMP UnlockStore() override;

    STDMETHODIMP GetCount(
        UINT32* count
    ) override;

    STDMETHODIMP GetItemByIndex(
        UINT32 index,
        GUID* guidKey,
        PROPVARIANT* value
    ) override;

    STDMETHODIMP CopyAllItems(
        IMFAttributes* destination
    ) override;

    HRESULT Initialize(
        const VirtualCameraFormat& format =
        VirtualCameraFormat{}
    );

private:
    ~VirtualCameraMediaSourceActivate();

    std::atomic<ULONG> m_refCount{ 1 };

    VirtualCameraFormat m_format{};

    IMFAttributes* m_attributes = nullptr;
    IMFMediaSource* m_mediaSource = nullptr;
};