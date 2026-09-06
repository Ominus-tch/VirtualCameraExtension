#pragma once

#include <mfidl.h>

#include <atomic>

#include "VirtualCameraFormat.h"

class VirtualCameraSource;

class VirtualCameraStream final
    : public IMFMediaStream2
{
public:
    VirtualCameraStream(
        VirtualCameraSource* source,
        DWORD streamId,
        const VirtualCameraFormat& format
    );

    // IUnknown
    STDMETHODIMP QueryInterface(
        REFIID riid,
        void** ppv
    ) override;

    STDMETHODIMP_(ULONG) AddRef() override;

    STDMETHODIMP_(ULONG) Release() override;

    // IMFMediaEventGenerator
    STDMETHODIMP GetEvent(
        DWORD flags,
        IMFMediaEvent** event
    ) override;

    STDMETHODIMP BeginGetEvent(
        IMFAsyncCallback* callback,
        IUnknown* state
    ) override;

    STDMETHODIMP EndGetEvent(
        IMFAsyncResult* result,
        IMFMediaEvent** event
    ) override;

    STDMETHODIMP QueueEvent(
        MediaEventType type,
        REFGUID extendedType,
        HRESULT status,
        const PROPVARIANT* value
    ) override;

    // IMFMediaStream
    STDMETHODIMP GetMediaSource(
        IMFMediaSource** source
    ) override;

    STDMETHODIMP GetStreamDescriptor(
        IMFStreamDescriptor** descriptor
    ) override;

    STDMETHODIMP RequestSample(
        IUnknown* token
    ) override;

    // IMFMediaStream2
    STDMETHODIMP SetStreamState(
        MF_STREAM_STATE state
    ) override;

    STDMETHODIMP GetStreamState(
        MF_STREAM_STATE* state
    ) override;

    // Internal
    HRESULT Start();
    HRESULT Stop();
    HRESULT Shutdown();

    HRESULT GetAttributes(
        IMFAttributes** attributes
    );

private:
    ~VirtualCameraStream();

    std::atomic<ULONG> m_refCount{ 1 };

    VirtualCameraSource* m_source = nullptr;

    DWORD m_streamId = 0;

    UINT32 m_width = 640;
    UINT32 m_height = 480;

    UINT32 m_frameRateNumerator = 30;
    UINT32 m_frameRateDenominator = 1;

    IMFStreamDescriptor* m_streamDescriptor = nullptr;
    IMFMediaEventQueue* m_eventQueue = nullptr;
    IMFAttributes* m_attributes = nullptr;

    MF_STREAM_STATE m_state =
        MF_STREAM_STATE_STOPPED;

    bool m_shutdown = false;
};