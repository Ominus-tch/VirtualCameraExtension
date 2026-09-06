#include "VirtualCameraStream.h"
#include "VirtualCameraExtension.h"
#include "VirtualCameraFormat.h"

#include <mfapi.h>
#include <mferror.h>

#include <ks.h>
#include <ksmedia.h>

#include <cwchar>

#include <cstdio>

namespace
{
    void LogToFileStream(
        const wchar_t* message)
    {
        FILE* file = nullptr;

        _wfopen_s(
            &file,
            L"C:\\Temp\\PianoVisualizerVirtualCamera.log",
            L"a, ccs=UTF-8"
        );

        if (!file)
            return;

        fwprintf(
            file,
            L"%s",
            message
        );

        fclose(file);
    }
}

VirtualCameraStream::VirtualCameraStream(
    VirtualCameraSource* source,
    DWORD streamId,
    const VirtualCameraFormat& format)
    : m_source(source),
    m_streamId(streamId),
    m_width(format.width),
    m_height(format.height),
    m_frameRateNumerator(format.frameRateNumerator),
    m_frameRateDenominator(format.frameRateDenominator)
{
    if (m_source)
        m_source->AddRef();

    MFCreateEventQueue(
        &m_eventQueue
    );

    MFCreateAttributes(
        &m_attributes,
        4
    );

    IMFMediaType* mediaType = nullptr;

    HRESULT hr =
        MFCreateMediaType(
            &mediaType
        );

    if (FAILED(hr))
        return;

    mediaType->SetGUID(
        MF_MT_MAJOR_TYPE,
        MFMediaType_Video
    );

    mediaType->SetGUID(
        MF_MT_SUBTYPE,
        MFVideoFormat_RGB32
    );

    mediaType->SetUINT32(
        MF_MT_INTERLACE_MODE,
        MFVideoInterlace_Progressive
    );

    mediaType->SetUINT32(
        MF_MT_ALL_SAMPLES_INDEPENDENT,
        TRUE
    );

    MFSetAttributeSize(
        mediaType,
        MF_MT_FRAME_SIZE,
        m_width,
        m_height
    );

    MFSetAttributeRatio(
        mediaType,
        MF_MT_FRAME_RATE,
        m_frameRateNumerator,
        m_frameRateDenominator
    );

    MFSetAttributeRatio(
        mediaType,
        MF_MT_PIXEL_ASPECT_RATIO,
        1,
        1
    );

    const UINT64 bitrate =
        (static_cast<UINT64>(m_width) *
            m_height *
            4 *
            8 *
            m_frameRateNumerator) /
        m_frameRateDenominator;

    MFSetAttributeRatio(
        mediaType,
        MF_MT_AVG_BITRATE,
        static_cast<UINT32>(bitrate),
        1
    );

    IMFMediaType* mediaTypes[] =
    {
        mediaType
    };

    hr = MFCreateStreamDescriptor(
        m_streamId,
        1,
        mediaTypes,
        &m_streamDescriptor
    );

    if (FAILED(hr))
    {
        mediaType->Release();
        return;
    }

    if (m_attributes)
    {
        m_attributes->SetGUID(
            MF_DEVICESTREAM_STREAM_CATEGORY,
            PINNAME_VIDEO_CAPTURE
        );

        m_attributes->SetUINT32(
            MF_DEVICESTREAM_STREAM_ID,
            m_streamId
        );

        m_attributes->SetUINT32(
            MF_DEVICESTREAM_FRAMESERVER_SHARED,
            1
        );

        m_attributes->SetUINT32(
            MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES,
            MFFrameSourceTypes_Color
        );
    }

    /*
        Same attributes on the stream descriptor.
    */

    if (m_streamDescriptor)
    {
        m_streamDescriptor->SetGUID(
            MF_DEVICESTREAM_STREAM_CATEGORY,
            PINNAME_VIDEO_CAPTURE
        );

        m_streamDescriptor->SetUINT32(
            MF_DEVICESTREAM_STREAM_ID,
            m_streamId
        );

        m_streamDescriptor->SetUINT32(
            MF_DEVICESTREAM_FRAMESERVER_SHARED,
            1
        );

        m_streamDescriptor->SetUINT32(
            MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES,
            MFFrameSourceTypes_Color
        );

        IMFMediaTypeHandler* handler = nullptr;

        if (SUCCEEDED(
            m_streamDescriptor->
            GetMediaTypeHandler(&handler)))
        {
            handler->SetCurrentMediaType(
                mediaType
            );

            handler->Release();
        }
    }

    mediaType->Release();

}

VirtualCameraStream::~VirtualCameraStream()
{
    Shutdown();

    if (m_source)
    {
        m_source->Release();
        m_source = nullptr;
    }

    if (m_streamDescriptor)
    {
        m_streamDescriptor->Release();
        m_streamDescriptor = nullptr;
    }

    if (m_attributes)
    {
        m_attributes->Release();
        m_attributes = nullptr;
    }

    if (m_eventQueue)
    {
        m_eventQueue->Release();
        m_eventQueue = nullptr;
    }

}

STDMETHODIMP VirtualCameraStream::QueryInterface(
    REFIID riid,
    void** ppv)
{
    OutputDebugStringW(
        L"[VirtualCameraStream] QueryInterface()\n"
    );

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
        L"[VirtualCameraStream] QueryInterface: %s\n",
        iidString
    );

    OutputDebugStringW(message);

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(
            static_cast<IMFMediaStream*>(this)
            );
    }
    else if (riid == IID_IMFMediaEventGenerator)
    {
        *ppv = static_cast<IMFMediaEventGenerator*>(
            static_cast<IMFMediaStream*>(this)
            );
    }
    else if (riid == IID_IMFMediaStream)
    {
        *ppv = static_cast<IMFMediaStream*>(this);
    }
    else if (riid == IID_IMFMediaStream2)
    {
        *ppv = static_cast<IMFMediaStream2*>(this);
    }
    else
    {
        OutputDebugStringW(
            L"[VirtualCameraStream] QueryInterface FAILED\n"
        );

        return E_NOINTERFACE;
    }

    AddRef();

    OutputDebugStringW(
        L"[VirtualCameraStream] QueryInterface SUCCEEDED\n"
    );

    return S_OK;

}

STDMETHODIMP_(ULONG)
VirtualCameraStream::AddRef()
{
    return ++m_refCount;
}

STDMETHODIMP_(ULONG)
VirtualCameraStream::Release()
{
    ULONG count = --m_refCount;

    if (count == 0)
        delete this;

    return count;

}

STDMETHODIMP VirtualCameraStream::GetEvent(
    DWORD flags,
    IMFMediaEvent** event)
{
    if (!m_eventQueue)
        return MF_E_SHUTDOWN;

    return m_eventQueue->GetEvent(
        flags,
        event
    );

}

STDMETHODIMP VirtualCameraStream::BeginGetEvent(
    IMFAsyncCallback* callback,
    IUnknown* state)
{
    if (!m_eventQueue)
        return MF_E_SHUTDOWN;

    return m_eventQueue->BeginGetEvent(
        callback,
        state
    );

}

STDMETHODIMP VirtualCameraStream::EndGetEvent(
    IMFAsyncResult* result,
    IMFMediaEvent** event)
{
    if (!m_eventQueue)
        return MF_E_SHUTDOWN;

    return m_eventQueue->EndGetEvent(
        result,
        event
    );

}

STDMETHODIMP VirtualCameraStream::QueueEvent(
    MediaEventType type,
    REFGUID extendedType,
    HRESULT status,
    const PROPVARIANT* value)
{
    if (!m_eventQueue)
        return MF_E_SHUTDOWN;

    return m_eventQueue->QueueEventParamVar(
        type,
        extendedType,
        status,
        value
    );

}

STDMETHODIMP VirtualCameraStream::GetMediaSource(
    IMFMediaSource** source)
{
    OutputDebugStringW(
        L"[VirtualCameraStream] GetMediaSource()\n"
    );

    if (!source)
        return E_POINTER;

    *source = nullptr;

    if (m_shutdown || !m_source)
        return MF_E_SHUTDOWN;

    return m_source->QueryInterface(
        IID_PPV_ARGS(source)
    );

}

STDMETHODIMP VirtualCameraStream::GetStreamDescriptor(
    IMFStreamDescriptor** descriptor)
{
    OutputDebugStringW(
        L"[VirtualCameraStream] GetStreamDescriptor()\n"
    );

    if (!descriptor)
        return E_POINTER;

    *descriptor = nullptr;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!m_streamDescriptor)
        return E_UNEXPECTED;

    *descriptor = m_streamDescriptor;
    m_streamDescriptor->AddRef();

    return S_OK;

}

STDMETHODIMP VirtualCameraStream::RequestSample(
    IUnknown* token)
{
    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample ENTER. source=%p token=%p\n",
    //        m_source,
    //        token
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    if (m_shutdown)
    {
        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: m_shutdown == true\n"
        );

        return MF_E_SHUTDOWN;
    }

    if (m_state != MF_STREAM_STATE_RUNNING)
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraStream] RequestSample: invalid state=%d\n",
            static_cast<int>(m_state)
        );

        LogToFileStream(message);
        OutputDebugStringW(message);

        return MF_E_INVALIDREQUEST;
    }

    if (!m_source)
    {
        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: m_source == nullptr\n"
        );

        return MF_E_SHUTDOWN;
    }

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: calling source->AllocateSample()\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample: calling source->AllocateSample()\n"
    //);

    IMFSample* sample = nullptr;

    HRESULT hr = m_source->AllocateSample(
        &sample
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: AllocateSample returned 0x%08lX sample=%p\n",
    //        static_cast<unsigned long>(hr),
    //        sample
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    if (FAILED(hr))
    {
        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: AllocateSample FAILED\n"
        );

        return hr;
    }

    if (!sample)
    {
        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: AllocateSample returned S_OK but sample == nullptr\n"
        );

        return E_UNEXPECTED;
    }

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: sample allocated successfully\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample: sample allocated successfully\n"
    //);

    IMFMediaBuffer* buffer = nullptr;

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: calling sample->GetBufferByIndex(0)\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample: calling sample->GetBufferByIndex(0)\n"
    //);

    hr = sample->GetBufferByIndex(
        0,
        &buffer
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: GetBufferByIndex returned 0x%08lX buffer=%p\n",
    //        static_cast<unsigned long>(hr),
    //        buffer
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    if (FAILED(hr))
    {
        sample->Release();

        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: GetBufferByIndex FAILED\n"
        );

        return hr;
    }

    if (!buffer)
    {
        sample->Release();

        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: GetBufferByIndex returned S_OK but buffer == nullptr\n"
        );

        return E_UNEXPECTED;
    }

    IMFDXGIBuffer* dxgiBuffer = nullptr;

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: calling buffer->QueryInterface(IMFDXGIBuffer)\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample: calling buffer->QueryInterface(IMFDXGIBuffer)\n"
    //);

    hr = buffer->QueryInterface(
        IID_PPV_ARGS(&dxgiBuffer)
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: QI(IMFDXGIBuffer) returned 0x%08lX dxgiBuffer=%p\n",
    //        static_cast<unsigned long>(hr),
    //        dxgiBuffer
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    buffer->Release();
    buffer = nullptr;

    if (FAILED(hr))
    {
        sample->Release();

        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: QI(IMFDXGIBuffer) FAILED\n"
        );

        return hr;
    }

    if (!dxgiBuffer)
    {
        sample->Release();

        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: QI returned S_OK but dxgiBuffer == nullptr\n"
        );

        return E_UNEXPECTED;
    }

    ID3D11Texture2D* texture = nullptr;

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: calling dxgiBuffer->GetResource(ID3D11Texture2D)\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample: calling dxgiBuffer->GetResource(ID3D11Texture2D)\n"
    //);

    hr = dxgiBuffer->GetResource(
        IID_ID3D11Texture2D,
        reinterpret_cast<void**>(&texture)
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: GetResource returned 0x%08lX texture=%p\n",
    //        static_cast<unsigned long>(hr),
    //        texture
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    dxgiBuffer->Release();
    dxgiBuffer = nullptr;

    if (FAILED(hr))
    {
        sample->Release();

        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: GetResource FAILED\n"
        );

        return hr;
    }

    if (!texture)
    {
        sample->Release();

        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: GetResource returned S_OK but texture == nullptr\n"
        );

        return E_UNEXPECTED;
    }

    D3D11_TEXTURE2D_DESC textureDescription{};

    texture->GetDesc(
        &textureDescription
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: destination texture %ux%u format=%u usage=%u bind=0x%08X misc=0x%08X\n",
    //        textureDescription.Width,
    //        textureDescription.Height,
    //        static_cast<unsigned int>(
    //            textureDescription.Format
    //            ),
    //        static_cast<unsigned int>(
    //            textureDescription.Usage
    //            ),
    //        textureDescription.BindFlags,
    //        textureDescription.MiscFlags
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: calling CopySharedFrameToTexture()\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample: calling CopySharedFrameToTexture()\n"
    //);

    hr = m_source->CopySharedFrameToTexture(
        texture
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: CopySharedFrameToTexture returned 0x%08lX\n",
    //        static_cast<unsigned long>(hr)
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    texture->Release();
    texture = nullptr;

    if (FAILED(hr))
    {
        sample->Release();

        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: CopySharedFrameToTexture FAILED\n"
        );

        return hr;
    }

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: shared frame copied successfully\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample: shared frame copied successfully\n"
    //);

    const LONGLONG sampleTime =
        MFGetSystemTime();

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: sampleTime=%lld\n",
    //        sampleTime
    //    );

    //    LogToFileStream(message);
    //}

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: calling sample->SetSampleTime()\n"
    //);

    hr = sample->SetSampleTime(
        sampleTime
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: SetSampleTime returned 0x%08lX\n",
    //        static_cast<unsigned long>(hr)
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    if (FAILED(hr))
    {
        sample->Release();
        return hr;
    }

    const LONGLONG sampleDuration =
        (10LL * 1000LL * 1000LL *
            m_frameRateDenominator) /
        m_frameRateNumerator;

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: sampleDuration=%lld\n",
    //        sampleDuration
    //    );

    //    LogToFileStream(message);
    //}

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample: calling sample->SetSampleDuration()\n"
    //);

    hr = sample->SetSampleDuration(
        sampleDuration
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: SetSampleDuration returned 0x%08lX\n",
    //        static_cast<unsigned long>(hr)
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    if (FAILED(hr))
    {
        sample->Release();
        return hr;
    }

    if (token)
    {
        /*LogToFileStream(
            L"[VirtualCameraStream] RequestSample: token supplied, calling SetUnknown(MFSampleExtension_Token)\n"
        );*/

        hr = sample->SetUnknown(
            MFSampleExtension_Token,
            token
        );

        /*{
            wchar_t message[256]{};

            swprintf_s(
                message,
                ARRAYSIZE(message),
                L"[VirtualCameraStream] RequestSample: SetUnknown(token) returned 0x%08lX\n",
                static_cast<unsigned long>(hr)
            );

            LogToFileStream(message);
            OutputDebugStringW(message);
        }*/

        if (FAILED(hr))
        {
            sample->Release();
            return hr;
        }
    }
    //else
    //{
    //    LogToFileStream(
    //        L"[VirtualCameraStream] RequestSample: no token supplied\n"
    //    );
    //}

    /*LogToFileStream(
        L"[VirtualCameraStream] RequestSample: calling QueueEventParamUnk(MEMediaSample)\n"
    );

    OutputDebugStringW(
        L"[VirtualCameraStream] RequestSample: calling QueueEventParamUnk(MEMediaSample)\n"
    );*/

    hr = m_eventQueue->QueueEventParamUnk(
        MEMediaSample,
        GUID_NULL,
        S_OK,
        sample
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraStream] RequestSample: QueueEventParamUnk returned 0x%08lX\n",
    //        static_cast<unsigned long>(hr)
    //    );

    //    LogToFileStream(message);
    //    OutputDebugStringW(message);
    //}

    sample->Release();
    sample = nullptr;

    if (FAILED(hr))
    {
        LogToFileStream(
            L"[VirtualCameraStream] RequestSample: QueueEventParamUnk FAILED\n"
        );

        return hr;
    }

    //LogToFileStream(
    //    L"[VirtualCameraStream] RequestSample EXIT: S_OK\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraStream] RequestSample EXIT: S_OK\n"
    //);

    return S_OK;
}

STDMETHODIMP VirtualCameraStream::SetStreamState(
    MF_STREAM_STATE state)
{
    if (m_shutdown)
        return MF_E_SHUTDOWN;

    switch (state)
    {
    case MF_STREAM_STATE_RUNNING:
        m_state = state;
        return S_OK;

    case MF_STREAM_STATE_STOPPED:
        m_state = state;
        return S_OK;

    case MF_STREAM_STATE_PAUSED:
        return MF_E_INVALID_STATE_TRANSITION;

    default:
        return MF_E_INVALID_STATE_TRANSITION;
    }

}

STDMETHODIMP VirtualCameraStream::GetStreamState(
    MF_STREAM_STATE* state)
{
    if (!state)
        return E_POINTER;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    *state = m_state;

    return S_OK;

}

HRESULT VirtualCameraStream::Start()
{
    if (m_shutdown)
        return MF_E_SHUTDOWN;

    m_state = MF_STREAM_STATE_RUNNING;

    if (m_eventQueue)
    {
        return m_eventQueue->QueueEventParamVar(
            MEStreamStarted,
            GUID_NULL,
            S_OK,
            nullptr
        );
    }

    return S_OK;

}

HRESULT VirtualCameraStream::Stop()
{
    if (m_shutdown)
        return MF_E_SHUTDOWN;

    m_state = MF_STREAM_STATE_STOPPED;

    if (m_eventQueue)
    {
        return m_eventQueue->QueueEventParamVar(
            MEStreamStopped,
            GUID_NULL,
            S_OK,
            nullptr
        );
    }

    return S_OK;

}

HRESULT VirtualCameraStream::Shutdown()
{
    if (m_shutdown)
        return S_OK;

    m_shutdown = true;

    m_state = MF_STREAM_STATE_STOPPED;

    if (m_eventQueue)
    {
        m_eventQueue->Shutdown();
    }

    return S_OK;

}

HRESULT VirtualCameraStream::GetAttributes(
    IMFAttributes** attributes)
{
    if (!attributes)
        return E_POINTER;

    *attributes = nullptr;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!m_attributes)
        return E_UNEXPECTED;

    *attributes = m_attributes;
    m_attributes->AddRef();

    return S_OK;

}