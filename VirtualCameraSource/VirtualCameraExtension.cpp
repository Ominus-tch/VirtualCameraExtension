#define INITGUID

#include "VirtualCameraExtension.h"
#include "VirtualCameraStream.h"
#include "VirtualCameraFormat.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>

#include <ks.h>
#include <ksproxy.h>
#include <ksmedia.h>

#include <cwchar>
#include <cstring>

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

#include <initguid.h>

void LogToFile(const wchar_t* message)
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

void LogAllocatorInterfaces(
    IMFVideoSampleAllocator* allocator)
{
    if (!allocator)
    {
        LogToFile(
            L"[VirtualCameraSource] Allocator interface check: allocator == nullptr\n"
        );

        return;
    }

    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] Allocator interface check: allocator=%p\n",
            allocator
        );

        LogToFile(message);
    }

    IMFVideoSampleAllocatorEx* allocatorEx = nullptr;

    HRESULT hr = allocator->QueryInterface(
        IID_PPV_ARGS(&allocatorEx)
    );

    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource]   IMFVideoSampleAllocatorEx: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
    }

    if (allocatorEx)
    {
        allocatorEx->Release();
        allocatorEx = nullptr;
    }

    IMFVideoCaptureSampleAllocator* captureAllocator = nullptr;

    hr = allocator->QueryInterface(
        IID_PPV_ARGS(&captureAllocator)
    );

    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource]   IMFVideoCaptureSampleAllocator: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
    }

    if (captureAllocator)
    {
        captureAllocator->Release();
        captureAllocator = nullptr;
    }
}

namespace
{
    HRESULT CompileShader(
        const char* source,
        const char* entryPoint,
        const char* target,
        ID3DBlob** blob)
    {
        if (!source || !entryPoint || !target || !blob)
            return E_POINTER;

        *blob = nullptr;

        ID3DBlob* errorBlob = nullptr;

        HRESULT hr = D3DCompile(
            source,
            strlen(source),
            nullptr,
            nullptr,
            nullptr,
            entryPoint,
            target,
            D3DCOMPILE_ENABLE_STRICTNESS,
            0,
            blob,
            &errorBlob
        );

        if (FAILED(hr))
        {
            if (errorBlob)
            {
                LogToFile(
                    L"[VirtualCameraSource] Shader compilation failed\n"
                );

                OutputDebugStringA(
                    static_cast<const char*>(errorBlob->GetBufferPointer())
                );

                errorBlob->Release();
            }
        }

        return hr;
    }
}

namespace
{
    constexpr wchar_t SharedFrameName[] =
        L"Global\\PianoVisualizerVirtualCameraFrame";
}

DEFINE_GUID(
    IID_IKsControl,
    0x28F54685,
    0x06FD,
    0x11D2,
    0xB2, 0x7A, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96
);

VirtualCameraSource::VirtualCameraSource(
    IMFAttributes* activationAttributes,
    const VirtualCameraFormat& format)
    : m_format(format)
{
    if (!m_format.IsValid())
    {
        m_format = VirtualCameraFormat{};

        OutputDebugStringW(
            L"[VirtualCameraSource] Invalid format; using 640x480 @ 30 FPS\n"
        );
    }

    OutputDebugStringW(
        L"[VirtualCameraSource] Constructed\n"
    );

    HRESULT hr = MFCreateAttributes(
        &m_sourceAttributes,
        4
    );

    if (FAILED(hr))
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] MFCreateAttributes failed\n"
        );

        return;
    }

    if (activationAttributes)
    {
        hr = activationAttributes->CopyAllItems(
            m_sourceAttributes
        );

        if (FAILED(hr))
        {
            OutputDebugStringW(
                L"[VirtualCameraSource] CopyAllItems from activation failed\n"
            );

            return;
        }
    }

    IMFSensorProfileCollection* profileCollection = nullptr;
    IMFSensorProfile* legacyProfile = nullptr;

    hr = MFCreateSensorProfileCollection(
        &profileCollection
    );

    if (FAILED(hr))
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] MFCreateSensorProfileCollection failed\n"
        );

        return;
    }

    hr = MFCreateSensorProfile(
        KSCAMERAPROFILE_Legacy,
        0,
        nullptr,
        &legacyProfile
    );

    if (SUCCEEDED(hr))
    {
        wchar_t profileFilter[128]{};

        swprintf_s(
            profileFilter,
            ARRAYSIZE(profileFilter),
            L"((RES==;FRT<=%u,%u;SUT==))",
            m_format.frameRateNumerator,
            m_format.frameRateDenominator
        );

        hr = legacyProfile->AddProfileFilter(
            0,
            profileFilter
        );
    }

    if (SUCCEEDED(hr))
    {
        hr = profileCollection->AddProfile(
            legacyProfile
        );
    }

    if (SUCCEEDED(hr))
    {
        hr = m_sourceAttributes->SetUnknown(
            MF_DEVICEMFT_SENSORPROFILE_COLLECTION,
            profileCollection
        );
    }

    if (legacyProfile)
        legacyProfile->Release();

    if (profileCollection)
        profileCollection->Release();

    if (FAILED(hr))
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Failed to create legacy sensor profile\n"
        );

        return;
    }

    OutputDebugStringW(
        L"[VirtualCameraSource] Legacy sensor profile created\n"
    );

    /*
        Create source event queue.
    */

    hr = MFCreateEventQueue(
        &m_eventQueue
    );

    if (FAILED(hr))
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] MFCreateEventQueue failed\n"
        );

        return;
    }

    /*
        Create our single video stream.
    */

    m_stream =
        new VirtualCameraStream(
            this,
            0,
            m_format
        );

    if (!m_stream)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Failed to create stream\n"
        );

        return;
    }

    /*
        Get the stream descriptor.
    */

    IMFStreamDescriptor* descriptor = nullptr;

    hr =
        m_stream->GetStreamDescriptor(
            &descriptor
        );

    if (FAILED(hr))
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] GetStreamDescriptor failed\n"
        );

        return;
    }

    /*
        Create presentation descriptor.
    */

    IMFStreamDescriptor* descriptors[] =
    {
        descriptor
    };

    hr = MFCreatePresentationDescriptor(
        1,
        descriptors,
        &m_presentationDescriptor
    );

    descriptor->Release();

    if (FAILED(hr))
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] MFCreatePresentationDescriptor failed\n"
        );

        return;
    }

    /*
        Create a video sample allocator.

        This gives the source a local allocator that can be
        used until the camera pipeline provides its own allocator
        through IMFSampleAllocatorControl::SetDefaultAllocator().
    */

    hr = MFCreateVideoSampleAllocatorEx(
        IID_IMFVideoSampleAllocator,
        reinterpret_cast<void**>(
            &m_sampleAllocator
            )
    );

    if (FAILED(hr))
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] MFCreateVideoSampleAllocatorEx failed\n"
        );

        /*
            Don't fail source creation here.

            The system may provide an allocator later through
            SetDefaultAllocator().
        */
    }
    else
    {
        m_sampleAllocatorOwned = true;

        OutputDebugStringW(
            L"[VirtualCameraSource] Sample allocator created\n"
        );
    }

    /*
        Create our own D3D11 device and DXGI device manager.

        Frame Server does not reliably call SetD3DManager(), so the
        virtual camera cannot depend on an externally supplied D3D
        manager. The source owns this device/manager instead.
    */
    hr = InitializeD3D();

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] InitializeD3D failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        OutputDebugStringW(message);
        LogToFile(message);
    }

    OutputDebugStringW(
        L"[VirtualCameraSource] Construction completed\n"
    );
}

VirtualCameraSource::~VirtualCameraSource()
{
    OutputDebugStringW(
        L"[VirtualCameraSource] DESTRUCTOR\n"
    );

    Shutdown();

    if (m_sampleAllocator)
    {
        m_sampleAllocator->Release();
        m_sampleAllocator = nullptr;
    }

    OutputDebugStringW(
        L"[VirtualCameraSource] DESTRUCTOR DONE\n"
    );
}


/*
    IUnknown
*/

STDMETHODIMP VirtualCameraSource::QueryInterface(
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
        ARRAYSIZE(message),
        L"[VirtualCameraSource] QueryInterface: %s\n",
        iidString
    );

    OutputDebugStringW(message);

    if (riid == IID_IUnknown)
    {
        *ppv =
            static_cast<IUnknown*>(
                static_cast<IMFMediaSourceEx*>(this)
                );
    }
    else if (riid == IID_IMFMediaEventGenerator)
    {
        *ppv =
            static_cast<IMFMediaEventGenerator*>(
                static_cast<IMFMediaSourceEx*>(this)
                );
    }
    else if (riid == IID_IMFMediaSource)
    {
        *ppv =
            static_cast<IMFMediaSource*>(
                static_cast<IMFMediaSourceEx*>(this)
                );
    }
    else if (riid == IID_IMFMediaSourceEx)
    {
        *ppv =
            static_cast<IMFMediaSourceEx*>(
                this
                );
    }
    else if (riid == IID_IMFGetService)
    {
        *ppv =
            static_cast<IMFGetService*>(
                this
                );
    }
    else if (riid == IID_IKsControl)
    {
        *ppv =
            static_cast<IKsControl*>(
                this
                );
    }
    else if (riid == IID_IMFSampleAllocatorControl)
    {
        *ppv =
            static_cast<IMFSampleAllocatorControl*>(
                this
                );
    }
    else
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] QueryInterface FAILED\n"
        );

        return E_NOINTERFACE;
    }

    AddRef();

    OutputDebugStringW(
        L"[VirtualCameraSource] QueryInterface SUCCEEDED\n"
    );

    return S_OK;
}

STDMETHODIMP_(ULONG)
VirtualCameraSource::AddRef()
{
    return ++m_refCount;
}

STDMETHODIMP_(ULONG)
VirtualCameraSource::Release()
{
    ULONG count = --m_refCount;

    if (count == 0)
        delete this;

    return count;
}


/*
    IMFMediaEventGenerator
*/

STDMETHODIMP VirtualCameraSource::GetEvent(
    DWORD flags,
    IMFMediaEvent** event)
{
    if (!event)
        return E_POINTER;

    *event = nullptr;

    if (!m_eventQueue)
        return MF_E_SHUTDOWN;

    return m_eventQueue->GetEvent(
        flags,
        event
    );
}

STDMETHODIMP VirtualCameraSource::BeginGetEvent(
    IMFAsyncCallback* callback,
    IUnknown* state)
{
    if (!callback)
        return E_POINTER;

    if (!m_eventQueue)
        return MF_E_SHUTDOWN;

    return m_eventQueue->BeginGetEvent(
        callback,
        state
    );
}

STDMETHODIMP VirtualCameraSource::EndGetEvent(
    IMFAsyncResult* result,
    IMFMediaEvent** event)
{
    if (!result)
        return E_INVALIDARG;

    if (!event)
        return E_POINTER;

    *event = nullptr;

    if (!m_eventQueue)
        return MF_E_SHUTDOWN;

    return m_eventQueue->EndGetEvent(
        result,
        event
    );
}

STDMETHODIMP VirtualCameraSource::QueueEvent(
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


/*
    IMFMediaSource
*/

STDMETHODIMP VirtualCameraSource::GetCharacteristics(
    DWORD* characteristics)
{
    if (!characteristics)
        return E_POINTER;

    *characteristics = 0;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    /*
        Virtual cameras are live sources.
    */

    *characteristics =
        MFMEDIASOURCE_IS_LIVE;

    return S_OK;
}

STDMETHODIMP
VirtualCameraSource::CreatePresentationDescriptor(
    IMFPresentationDescriptor** presentationDescriptor)
{
    OutputDebugStringW(
        L"[VirtualCameraSource] CreatePresentationDescriptor()\n"
    );

    if (!presentationDescriptor)
        return E_POINTER;

    *presentationDescriptor = nullptr;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!m_presentationDescriptor)
        return E_UNEXPECTED;

    return m_presentationDescriptor->Clone(
        presentationDescriptor
    );
}

STDMETHODIMP VirtualCameraSource::Start(
    IMFPresentationDescriptor* presentationDescriptor,
    const GUID* timeFormat,
    const PROPVARIANT* startPosition)
{
    OutputDebugStringW(
        L"[VirtualCameraSource] Start()\n"
    );

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!presentationDescriptor)
        return E_INVALIDARG;

    if (!startPosition)
        return E_INVALIDARG;

    /*
        We only support the default time format.
    */

    if (timeFormat &&
        *timeFormat != GUID_NULL)
    {
        return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    DWORD streamCount = 0;

    HRESULT hr =
        presentationDescriptor->
        GetStreamDescriptorCount(
            &streamCount
        );

    if (FAILED(hr))
        return hr;

    if (streamCount != 1)
        return E_INVALIDARG;

    BOOL selected = FALSE;

    IMFStreamDescriptor* descriptor = nullptr;

    hr =
        presentationDescriptor->
        GetStreamDescriptorByIndex(
            0,
            &selected,
            &descriptor
        );

    if (FAILED(hr))
        return hr;

    descriptor->Release();

    if (!selected)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Stream not selected\n"
        );

        return E_INVALIDARG;
    }

    if (!m_stream)
        return MF_E_SHUTDOWN;

    /*
        Start stream first.
    */

    hr = m_stream->Start();

    if (FAILED(hr))
        return hr;

    /*
        Notify Media Foundation that our stream exists.
    */

    hr = m_eventQueue->QueueEventParamUnk(
        MENewStream,
        GUID_NULL,
        S_OK,
        m_stream
    );

    if (FAILED(hr))
        return hr;

    /*
        Tell the pipeline that the source has started.
    */

    hr = m_eventQueue->QueueEventParamVar(
        MESourceStarted,
        GUID_NULL,
        S_OK,
        startPosition
    );

    if (FAILED(hr))
        return hr;

    OutputDebugStringW(
        L"[VirtualCameraSource] Start() completed\n"
    );

    return S_OK;
}

STDMETHODIMP VirtualCameraSource::Stop()
{
    OutputDebugStringW(
        L"[VirtualCameraSource] Stop()\n"
    );

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (m_stream)
    {
        HRESULT hr = m_stream->Stop();

        if (FAILED(hr))
            return hr;
    }

    if (m_eventQueue)
    {
        return m_eventQueue->QueueEventParamVar(
            MESourceStopped,
            GUID_NULL,
            S_OK,
            nullptr
        );
    }

    return S_OK;
}

STDMETHODIMP VirtualCameraSource::Pause()
{
    OutputDebugStringW(
        L"[VirtualCameraSource] Pause()\n"
    );

    return MF_E_INVALID_STATE_TRANSITION;
}

STDMETHODIMP VirtualCameraSource::Shutdown()
{
    wchar_t message[256]{};

    swprintf_s(
        message,
        L"[VirtualCameraSource] !!! SHUTDOWN CALLED !!! TID=%lu this=%p\n",
        GetCurrentThreadId(),
        this
    );

    OutputDebugStringW(message);

    if (m_shutdown)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Already shut down\n"
        );

        return S_OK;
    }

    m_shutdown = true;

    OutputDebugStringW(
        L"[VirtualCameraSource] m_shutdown = true\n"
    );

    if (m_stream)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Shutting down stream\n"
        );

        HRESULT hr = m_stream->Shutdown();

        swprintf_s(
            message,
            L"[VirtualCameraSource] stream->Shutdown() = 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        OutputDebugStringW(message);

        m_stream->Release();
        m_stream = nullptr;
    }

    if (m_presentationDescriptor)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Releasing presentation descriptor\n"
        );

        m_presentationDescriptor->Release();
        m_presentationDescriptor = nullptr;
    }

    if (m_sampleAllocator)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Releasing sample allocator\n"
        );

        m_sampleAllocator->Release();
        m_sampleAllocator = nullptr;
    }

    if (m_sourceAttributes)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Releasing source attributes\n"
        );

        m_sourceAttributes->Release();
        m_sourceAttributes = nullptr;
    }

    if (m_eventQueue)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] Shutting down event queue\n"
        );

        m_eventQueue->Shutdown();
        m_eventQueue->Release();
        m_eventQueue = nullptr;
    }

    if (m_sharedMutex)
    {
        LogToFile(
            L"[VirtualCameraSource] Releasing shared texture mutex\n"
        );

        m_sharedMutex->Release();
        m_sharedMutex = nullptr;
    }

    if (m_sharedTexture)
    {
        LogToFile(
            L"[VirtualCameraSource] Releasing shared texture\n"
        );

        m_sharedTexture->Release();
        m_sharedTexture = nullptr;
    }

    ReleaseScalingResources();

    if (m_d3dContext)
    {
        LogToFile(
            L"[VirtualCameraSource] Releasing D3D11 context\n"
        );

        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }

    if (m_d3dDevice)
    {
        LogToFile(
            L"[VirtualCameraSource] Releasing D3D11 device\n"
        );

        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }

    if (m_dxgiManager)
    {
        LogToFile(
            L"[VirtualCameraSource] Releasing self-owned DXGI manager\n"
        );

        m_dxgiManager->Release();
        m_dxgiManager = nullptr;
    }

    OutputDebugStringW(
        L"[VirtualCameraSource] Shutdown completed\n"
    );

    return S_OK;
}


/*
    IMFMediaSourceEx
*/

STDMETHODIMP VirtualCameraSource::GetSourceAttributes(
    IMFAttributes** attributes)
{
    OutputDebugStringW(
        L"[VirtualCameraSource] GetSourceAttributes()\n"
    );

    if (!attributes)
        return E_POINTER;

    *attributes = nullptr;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!m_sourceAttributes)
        return E_UNEXPECTED;

    *attributes = m_sourceAttributes;

    m_sourceAttributes->AddRef();

    return S_OK;
}

STDMETHODIMP VirtualCameraSource::GetStreamAttributes(
    DWORD streamIdentifier,
    IMFAttributes** attributes)
{
    OutputDebugStringW(
        L"[VirtualCameraSource] GetStreamAttributes()\n"
    );

    if (!attributes)
        return E_POINTER;

    *attributes = nullptr;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (streamIdentifier != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!m_stream)
        return MF_E_SHUTDOWN;

    return m_stream->GetAttributes(
        attributes
    );
}

HRESULT VirtualCameraSource::InitializeD3D()
{
    LogToFile(
        L"[VirtualCameraSource] InitializeD3D ENTER\n"
    );

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (m_d3dDevice && m_d3dContext && m_dxgiManager)
    {
        LogToFile(
            L"[VirtualCameraSource] InitializeD3D: already initialized\n"
        );

        return S_OK;
    }

    if (m_sharedMutex)
    {
        m_sharedMutex->Release();
        m_sharedMutex = nullptr;
    }

    if (m_sharedTexture)
    {
        m_sharedTexture->Release();
        m_sharedTexture = nullptr;
    }

    if (m_d3dContext)
    {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }

    if (m_d3dDevice)
    {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }

    if (m_dxgiManager)
    {
        m_dxgiManager->Release();
        m_dxgiManager = nullptr;
    }

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &featureLevel,
        &m_d3dContext
    );

    if (hr == E_INVALIDARG)
    {
        /*
            Some systems reject D3D_FEATURE_LEVEL_11_1 when the
            corresponding runtime interface is unavailable.
        */
        D3D_FEATURE_LEVEL fallbackLevels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };

        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creationFlags,
            fallbackLevels,
            ARRAYSIZE(fallbackLevels),
            D3D11_SDK_VERSION,
            &m_d3dDevice,
            &featureLevel,
            &m_d3dContext
        );
    }

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] D3D11CreateDevice failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        return hr;
    }

    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] D3D11 device created. device=%p context=%p featureLevel=0x%04X\n",
            m_d3dDevice,
            m_d3dContext,
            static_cast<unsigned int>(featureLevel)
        );

        LogToFile(message);
        OutputDebugStringW(message);
    }

    UINT resetToken = 0;

    hr = MFCreateDXGIDeviceManager(
        &resetToken,
        &m_dxgiManager
    );

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] MFCreateDXGIDeviceManager failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        return hr;
    }

    hr = m_dxgiManager->ResetDevice(
        m_d3dDevice,
        resetToken
    );

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] IMFDXGIDeviceManager::ResetDevice failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        return hr;
    }

    LogToFile(
        L"[VirtualCameraSource] Self-owned DXGI device manager initialized\n"
    );

    /*
        Connect our locally-created allocator to our self-owned
        DXGI manager. The allocator must receive the manager before
        InitializeSampleAllocatorEx().
    */
    if (m_sampleAllocator)
    {
        hr = m_sampleAllocator->SetDirectXManager(
            m_dxgiManager
        );

        if (FAILED(hr))
        {
            wchar_t message[256]{};

            swprintf_s(
                message,
                ARRAYSIZE(message),
                L"[VirtualCameraSource] SetDirectXManager(local allocator) failed: 0x%08lX\n",
                static_cast<unsigned long>(hr)
            );

            LogToFile(message);
            OutputDebugStringW(message);

            return hr;
        }

        LogToFile(
            L"[VirtualCameraSource] Local allocator connected to self-owned DXGI manager\n"
        );

        IMFVideoSampleAllocatorEx* allocatorEx = nullptr;

        hr = m_sampleAllocator->QueryInterface(
            IID_PPV_ARGS(&allocatorEx)
        );

        if (SUCCEEDED(hr))
        {
            IMFStreamDescriptor* descriptor = nullptr;
            IMFMediaTypeHandler* handler = nullptr;
            IMFMediaType* mediaType = nullptr;
            IMFAttributes* allocatorAttributes = nullptr;

            hr = m_stream->GetStreamDescriptor(
                &descriptor
            );

            if (SUCCEEDED(hr))
            {
                hr = descriptor->GetMediaTypeHandler(
                    &handler
                );
            }

            if (SUCCEEDED(hr))
            {
                hr = handler->GetCurrentMediaType(
                    &mediaType
                );
            }

            if (SUCCEEDED(hr))
            {
                hr = MFCreateAttributes(
                    &allocatorAttributes,
                    2
                );
            }

            if (SUCCEEDED(hr))
            {
                hr = allocatorAttributes->SetUINT32(
                    MF_SA_D3D11_USAGE,
                    D3D11_USAGE_DEFAULT
                );
            }

            if (SUCCEEDED(hr))
            {
                hr = allocatorAttributes->SetUINT32(
                    MF_SA_D3D11_BINDFLAGS,
                    0
                );
            }

            if (SUCCEEDED(hr))
            {
                LogToFile(
                    L"[VirtualCameraSource] Initializing local DXGI sample allocator...\n"
                );

                hr = allocatorEx->InitializeSampleAllocatorEx(
                    2,
                    4,
                    allocatorAttributes,
                    mediaType
                );
            }

            if (allocatorAttributes)
                allocatorAttributes->Release();

            if (mediaType)
                mediaType->Release();

            if (handler)
                handler->Release();

            if (descriptor)
                descriptor->Release();

            allocatorEx->Release();
            allocatorEx = nullptr;

            if (FAILED(hr))
            {
                wchar_t message[256]{};

                swprintf_s(
                    message,
                    ARRAYSIZE(message),
                    L"[VirtualCameraSource] InitializeSampleAllocatorEx(local allocator) failed: 0x%08lX\n",
                    static_cast<unsigned long>(hr)
                );

                LogToFile(message);
                OutputDebugStringW(message);

                return hr;
            }

            m_sampleAllocatorInitialized = true;

            LogToFile(
                L"[VirtualCameraSource] Local DXGI sample allocator initialized successfully\n"
            );
        }
        else
        {
            wchar_t message[256]{};

            swprintf_s(
                message,
                ARRAYSIZE(message),
                L"[VirtualCameraSource] Local allocator does not expose IMFVideoSampleAllocatorEx: 0x%08lX\n",
                static_cast<unsigned long>(hr)
            );

            LogToFile(message);
            OutputDebugStringW(message);

            /*
                Keep the allocator. SetDirectXManager() succeeded, so
                leave it available for diagnostics rather than failing
                source creation solely because Ex is unavailable.
            */
            hr = S_OK;
        }
    }

    LogToFile(
        L"[VirtualCameraSource] InitializeD3D EXIT: S_OK\n"
    );

    return S_OK;
}

HRESULT VirtualCameraSource::InitializeScalingResources(
    const D3D11_TEXTURE2D_DESC& sharedDescription)
{
    if (!m_d3dDevice)
        return MF_E_NOT_INITIALIZED;

    if (sharedDescription.Width == 0 ||
        sharedDescription.Height == 0)
    {
        return E_INVALIDARG;
    }

    /*
        Reuse the existing scaling resources when the shared texture
        dimensions and format have not changed.
    */

    if (m_scaleTexture &&
        m_scaleSRV &&
        m_scaleRTV &&
        m_scaleTextureWidth == sharedDescription.Width &&
        m_scaleTextureHeight == sharedDescription.Height &&
        m_scaleTextureFormat == sharedDescription.Format)
    {
        return S_OK;
    }

    ReleaseScalingResources();

    /*
        The intermediate texture receives the PianoVisualizer texture
        at its original size and is then sampled by the scaling shader.
    */

    D3D11_TEXTURE2D_DESC scaleDescription = {};

    scaleDescription.Width = sharedDescription.Width;
    scaleDescription.Height = sharedDescription.Height;
    scaleDescription.MipLevels = 1;
    scaleDescription.ArraySize = 1;
    scaleDescription.Format = sharedDescription.Format;
    scaleDescription.SampleDesc.Count = 1;
    scaleDescription.SampleDesc.Quality = 0;
    scaleDescription.Usage = D3D11_USAGE_DEFAULT;
    scaleDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    scaleDescription.CPUAccessFlags = 0;
    scaleDescription.MiscFlags = 0;

    HRESULT hr = m_d3dDevice->CreateTexture2D(
        &scaleDescription,
        nullptr,
        &m_scaleTexture
    );

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] CreateTexture2D(scale texture) failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        ReleaseScalingResources();

        return hr;
    }

    hr = m_d3dDevice->CreateShaderResourceView(
        m_scaleTexture,
        nullptr,
        &m_scaleSRV
    );

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] CreateShaderResourceView(scale texture) failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        ReleaseScalingResources();

        return hr;
    }

    /*
        Fullscreen-triangle vertex shader.

        It generates both position and UV coordinates entirely from
        SV_VertexID, so no vertex buffer is required.
    */

    static constexpr char VertexShaderSource[] = R"(
        struct VSOutput
        {
            float4 position : SV_Position;
            float2 uv       : TEXCOORD0;
        };

        VSOutput main(uint vertexID : SV_VertexID)
        {
            VSOutput output;

            if (vertexID == 0)
            {
                output.position = float4(-1.0, -1.0, 0.0, 1.0);
                output.uv = float2(0.0, 1.0);
            }
            else if (vertexID == 1)
            {
                output.position = float4(-1.0, 3.0, 0.0, 1.0);
                output.uv = float2(0.0, -1.0);
            }
            else
            {
                output.position = float4(3.0, -1.0, 0.0, 1.0);
                output.uv = float2(2.0, 1.0);
            }

            return output;
        }
    )";

    static constexpr char PixelShaderSource[] = R"(
        Texture2D frameTexture : register(t0);
        SamplerState frameSampler : register(s0);

        struct PSInput
        {
            float4 position : SV_Position;
            float2 uv       : TEXCOORD0;
        };

        float4 main(PSInput input) : SV_Target
        {
            return frameTexture.Sample(
                frameSampler,
                input.uv
            );
        }
    )";

    ID3DBlob* vertexShaderBlob = nullptr;

    hr = CompileShader(
        VertexShaderSource,
        "main",
        "vs_5_0",
        &vertexShaderBlob
    );

    if (FAILED(hr))
    {
        ReleaseScalingResources();
        return hr;
    }

    hr = m_d3dDevice->CreateVertexShader(
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        nullptr,
        &m_scaleVertexShader
    );

    vertexShaderBlob->Release();
    vertexShaderBlob = nullptr;

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] CreateVertexShader failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        ReleaseScalingResources();

        return hr;
    }

    ID3DBlob* pixelShaderBlob = nullptr;

    hr = CompileShader(
        PixelShaderSource,
        "main",
        "ps_5_0",
        &pixelShaderBlob
    );

    if (FAILED(hr))
    {
        ReleaseScalingResources();
        return hr;
    }

    hr = m_d3dDevice->CreatePixelShader(
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize(),
        nullptr,
        &m_scalePixelShader
    );

    pixelShaderBlob->Release();
    pixelShaderBlob = nullptr;

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] CreatePixelShader failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        ReleaseScalingResources();

        return hr;
    }

    D3D11_SAMPLER_DESC samplerDescription = {};

    samplerDescription.Filter =
        D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    samplerDescription.AddressU =
        D3D11_TEXTURE_ADDRESS_CLAMP;

    samplerDescription.AddressV =
        D3D11_TEXTURE_ADDRESS_CLAMP;

    samplerDescription.AddressW =
        D3D11_TEXTURE_ADDRESS_CLAMP;

    samplerDescription.MipLODBias = 0.0f;
    samplerDescription.MaxAnisotropy = 1;
    samplerDescription.ComparisonFunc =
        D3D11_COMPARISON_ALWAYS;

    samplerDescription.MinLOD = 0.0f;
    samplerDescription.MaxLOD =
        D3D11_FLOAT32_MAX;

    hr = m_d3dDevice->CreateSamplerState(
        &samplerDescription,
        &m_scaleSampler
    );

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] CreateSamplerState failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        ReleaseScalingResources();

        return hr;
    }

    /*
        The destination is created dynamically by the MF allocator.
        Its RTV is therefore created in CopySharedFrameToTexture().
    */

    m_scaleTextureWidth =
        sharedDescription.Width;

    m_scaleTextureHeight =
        sharedDescription.Height;

    m_scaleTextureFormat =
        sharedDescription.Format;

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraSource] Scaling resources initialized for %ux%u format=%u\n",
    //        m_scaleTextureWidth,
    //        m_scaleTextureHeight,
    //        static_cast<unsigned int>(
    //            m_scaleTextureFormat
    //            )
    //    );

    //    LogToFile(message);
    //    OutputDebugStringW(message);
    //}

    return S_OK;
}

void VirtualCameraSource::ReleaseScalingResources()
{
    if (m_scaleRTV)
    {
        m_scaleRTV->Release();
        m_scaleRTV = nullptr;
    }

    if (m_scaleSampler)
    {
        m_scaleSampler->Release();
        m_scaleSampler = nullptr;
    }

    if (m_scalePixelShader)
    {
        m_scalePixelShader->Release();
        m_scalePixelShader = nullptr;
    }

    if (m_scaleVertexShader)
    {
        m_scaleVertexShader->Release();
        m_scaleVertexShader = nullptr;
    }

    if (m_scaleSRV)
    {
        m_scaleSRV->Release();
        m_scaleSRV = nullptr;
    }

    if (m_scaleTexture)
    {
        m_scaleTexture->Release();
        m_scaleTexture = nullptr;
    }

    m_scaleTextureWidth = 0;
    m_scaleTextureHeight = 0;
    m_scaleTextureFormat = DXGI_FORMAT_UNKNOWN;
}

STDMETHODIMP VirtualCameraSource::SetD3DManager(
    IUnknown* manager)
{
    wchar_t message[256]{};

    swprintf_s(
        message,
        ARRAYSIZE(message),
        L"[VirtualCameraSource %p] SetD3DManager called. manager=%p (ignored; source owns D3D device)\n",
        this,
        manager
    );

    LogToFile(message);
    OutputDebugStringW(message);

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    /*
        The source owns its D3D11 device and IMFDXGIDeviceManager.
        Frame Server is not required to provide a D3D manager.
    */
    return S_OK;
}

HRESULT VirtualCameraSource::AllocateSample(
    IMFSample** sample)
{
    /*{
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource %p] AllocateSample ENTER. allocator=%p owned=%d initialized=%d\n",
            this,
            m_sampleAllocator,
            m_sampleAllocatorOwned ? 1 : 0,
            m_sampleAllocatorInitialized ? 1 : 0
        );

        LogToFile(message);
        OutputDebugStringW(message);
    }*/

    if (!sample)
    {
        LogToFile(
            L"[VirtualCameraSource] AllocateSample: sample == nullptr\n"
        );

        return E_POINTER;
    }

    *sample = nullptr;

    if (m_shutdown)
    {
        LogToFile(
            L"[VirtualCameraSource] AllocateSample: m_shutdown == true\n"
        );

        return MF_E_SHUTDOWN;
    }

    if (!m_sampleAllocator)
    {
        LogToFile(
            L"[VirtualCameraSource] AllocateSample: m_sampleAllocator == nullptr\n"
        );

        return MF_E_NOT_INITIALIZED;
    }

    //LogAllocatorInterfaces(
    //    m_sampleAllocator
    //);

    //LogToFile(
    //    L"[VirtualCameraSource] AllocateSample: calling m_sampleAllocator->AllocateSample()\n"
    //);

    HRESULT hr = m_sampleAllocator->AllocateSample(
        sample
    );

    //{
    //    wchar_t message[256]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraSource %p] AllocateSample returned 0x%08lX sample=%p\n",
    //        this,
    //        static_cast<unsigned long>(hr),
    //        sample ? *sample : nullptr
    //    );

    //    LogToFile(message);
    //    OutputDebugStringW(message);
    //}

    return hr;
}

HRESULT VirtualCameraSource::CopySharedFrameToTexture(
    ID3D11Texture2D* destination)
{
    if (!destination)
        return E_POINTER;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!m_d3dDevice || !m_d3dContext)
        return MF_E_NOT_INITIALIZED;

    /*
        The PianoVisualizer process may create the named shared texture
        after the virtual camera source itself has been constructed.
    */

    if (!m_sharedTexture || !m_sharedMutex)
    {
        ID3D11Device1* device1 = nullptr;

        HRESULT hr = m_d3dDevice->QueryInterface(
            IID_PPV_ARGS(&device1)
        );

        if (FAILED(hr))
            return hr;

        if (m_sharedMutex)
        {
            m_sharedMutex->Release();
            m_sharedMutex = nullptr;
        }

        if (m_sharedTexture)
        {
            m_sharedTexture->Release();
            m_sharedTexture = nullptr;
        }

        hr = device1->OpenSharedResourceByName(
            SharedFrameName,
            DXGI_SHARED_RESOURCE_READ |
            DXGI_SHARED_RESOURCE_WRITE,
            IID_ID3D11Texture2D,
            reinterpret_cast<void**>(&m_sharedTexture)
        );

        device1->Release();
        device1 = nullptr;

        if (FAILED(hr))
        {
            wchar_t message[256]{};

            swprintf_s(
                message,
                ARRAYSIZE(message),
                L"[VirtualCameraSource] OpenSharedResourceByName failed: 0x%08lX\n",
                static_cast<unsigned long>(hr)
            );

            LogToFile(message);
            OutputDebugStringW(message);

            return hr;
        }

        hr = m_sharedTexture->QueryInterface(
            IID_PPV_ARGS(&m_sharedMutex)
        );

        if (FAILED(hr))
        {
            wchar_t message[256]{};

            swprintf_s(
                message,
                ARRAYSIZE(message),
                L"[VirtualCameraSource] QueryInterface(IDXGIKeyedMutex) failed: 0x%08lX\n",
                static_cast<unsigned long>(hr)
            );

            LogToFile(message);
            OutputDebugStringW(message);

            m_sharedTexture->Release();
            m_sharedTexture = nullptr;

            return hr;
        }

        //LogToFile(
        //    L"[VirtualCameraSource] Shared PianoVisualizer texture opened successfully\n"
        //);
    }

    D3D11_TEXTURE2D_DESC sharedDescription{};
    D3D11_TEXTURE2D_DESC destinationDescription{};

    m_sharedTexture->GetDesc(
        &sharedDescription
    );

    destination->GetDesc(
        &destinationDescription
    );

    /*{
        wchar_t message[512]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] CopySharedFrameToTexture: "
            L"shared=%ux%u format=%u | destination=%ux%u format=%u\n",
            sharedDescription.Width,
            sharedDescription.Height,
            static_cast<unsigned int>(
                sharedDescription.Format
                ),
            destinationDescription.Width,
            destinationDescription.Height,
            static_cast<unsigned int>(
                destinationDescription.Format
                )
        );

        LogToFile(message);
        OutputDebugStringW(message);
    }*/

    /*
        The GPU scaling path expects the source and destination formats
        to be compatible with the pixel shader.

        PianoVisualizer uses BGRA/RGB32-compatible output.
    */

    //if (sharedDescription.Format !=
    //    destinationDescription.Format)
    //{
    //    wchar_t message[512]{};

    //    swprintf_s(
    //        message,
    //        ARRAYSIZE(message),
    //        L"[VirtualCameraSource] CopySharedFrameToTexture: "
    //        L"format mismatch. shared=%u destination=%u\n",
    //        static_cast<unsigned int>(
    //            sharedDescription.Format
    //            ),
    //        static_cast<unsigned int>(
    //            destinationDescription.Format
    //            )
    //    );

    //    LogToFile(message);
    //    OutputDebugStringW(message);
    //}

    /*
        Initialize or recreate the source-side scaling resources if the
        PianoVisualizer window/output resolution has changed.
    */

    HRESULT hr = InitializeScalingResources(
        sharedDescription
    );

    if (FAILED(hr))
        return hr;

    /*
        Acquire the PianoVisualizer shared texture.

        Key 1 = camera owns the texture.
        Key 0 = PianoVisualizer owns the texture.
    */

    //LogToFile(
    //    L"[VirtualCameraSource] AcquireSync(1)...\n"
    //);

    hr = m_sharedMutex->AcquireSync(
        1,
        100
    );

    if (hr == WAIT_TIMEOUT)
    {
        LogToFile(
            L"[VirtualCameraSource] AcquireSync timed out\n"
        );

        return HRESULT_FROM_WIN32(
            WAIT_TIMEOUT
        );
    }

    if (hr == WAIT_ABANDONED)
    {
        LogToFile(
            L"[VirtualCameraSource] AcquireSync returned WAIT_ABANDONED\n"
        );
    }
    else if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] AcquireSync failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        return hr;
    }

    /*
        Copy the shared texture into our SRV-readable intermediate
        texture. This copy does NOT require matching the camera
        destination dimensions.
    */

    m_d3dContext->CopyResource(
        m_scaleTexture,
        m_sharedTexture
    );

    /*
        Flush before releasing the keyed mutex so the source texture
        isn't handed back to PianoVisualizer before our copy has been
        submitted to the GPU.
    */

    m_d3dContext->Flush();

    hr = m_sharedMutex->ReleaseSync(
        0
    );

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] ReleaseSync failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        return hr;
    }

    /*
        Create an RTV for the allocator-provided destination texture.

        The Frame Server allocator gives us the destination texture
        dynamically on every sample.
    */

    if (m_scaleRTV)
    {
        m_scaleRTV->Release();
        m_scaleRTV = nullptr;
    }

    hr = m_d3dDevice->CreateRenderTargetView(
        destination,
        nullptr,
        &m_scaleRTV
    );

    if (FAILED(hr))
    {
        wchar_t message[256]{};

        swprintf_s(
            message,
            ARRAYSIZE(message),
            L"[VirtualCameraSource] CreateRenderTargetView(destination) failed: 0x%08lX\n",
            static_cast<unsigned long>(hr)
        );

        LogToFile(message);
        OutputDebugStringW(message);

        return hr;
    }

    /*
        Set the camera output viewport.

        This is the important part that allows a 2560x1440,
        1920x1080, 1280x720, etc. PianoVisualizer texture to be
        scaled to whatever dimensions the camera allocator requested.
    */

    D3D11_VIEWPORT viewport{};

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width =
        static_cast<float>(
            destinationDescription.Width
            );
    viewport.Height =
        static_cast<float>(
            destinationDescription.Height
            );
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    m_d3dContext->RSSetViewports(
        1,
        &viewport
    );

    /*
        Bind the destination camera texture as the render target.
    */

    m_d3dContext->OMSetRenderTargets(
        1,
        &m_scaleRTV,
        nullptr
    );

    /*
        Fullscreen triangle.

        No vertex buffer or input layout is required because the vertex
        shader generates the geometry from SV_VertexID.
    */

    m_d3dContext->IASetInputLayout(
        nullptr
    );

    m_d3dContext->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    m_d3dContext->VSSetShader(
        m_scaleVertexShader,
        nullptr,
        0
    );

    m_d3dContext->PSSetShader(
        m_scalePixelShader,
        nullptr,
        0
    );

    m_d3dContext->PSSetShaderResources(
        0,
        1,
        &m_scaleSRV
    );

    m_d3dContext->PSSetSamplers(
        0,
        1,
        &m_scaleSampler
    );

    /*
        Draw the fullscreen triangle.

        The pixel shader bilinearly samples the PianoVisualizer frame,
        automatically scaling it to the camera texture dimensions.
    */

    m_d3dContext->Draw(
        3,
        0
    );

    /*
        Unbind the SRV and render target so that the resources are not
        left simultaneously bound in conflicting pipeline stages.
    */

    ID3D11ShaderResourceView* nullSRV = nullptr;

    m_d3dContext->PSSetShaderResources(
        0,
        1,
        &nullSRV
    );

    ID3D11RenderTargetView* nullRTV = nullptr;

    m_d3dContext->OMSetRenderTargets(
        1,
        &nullRTV,
        nullptr
    );

    /*
        Release the per-destination RTV.

        The camera allocator owns the actual destination texture;
        m_scaleRTV is merely our temporary view of it.
    */

    m_scaleRTV->Release();
    m_scaleRTV = nullptr;

    //LogToFile(
    //    L"[VirtualCameraSource] GPU scaling completed successfully\n"
    //);

    //OutputDebugStringW(
    //    L"[VirtualCameraSource] GPU scaling completed successfully\n"
    //);

    return S_OK;
}

/*
    IMFGetService
*/

STDMETHODIMP VirtualCameraSource::GetService(
    REFGUID guidService,
    REFIID riid,
    LPVOID* ppvObject)
{
    if (!ppvObject)
        return E_POINTER;

    *ppvObject = nullptr;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    /*
        Currently no additional Media Foundation services
        are exposed by this source.
    */

    return MF_E_UNSUPPORTED_SERVICE;
}


/*
    IKsControl
*/

STDMETHODIMP VirtualCameraSource::KsProperty(
    PKSPROPERTY property,
    ULONG propertyLength,
    LPVOID propertyData,
    ULONG dataLength,
    ULONG* bytesReturned)
{
    if (bytesReturned)
        *bytesReturned = 0;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!property)
        return E_POINTER;

    /*
        We currently don't expose any KS properties.
    */

    return HRESULT_FROM_WIN32(
        ERROR_SET_NOT_FOUND
    );
}

STDMETHODIMP VirtualCameraSource::KsMethod(
    PKSMETHOD method,
    ULONG methodLength,
    LPVOID methodData,
    ULONG dataLength,
    ULONG* bytesReturned)
{
    if (bytesReturned)
        *bytesReturned = 0;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!method)
        return E_POINTER;

    /*
        We currently don't expose any KS methods.
    */

    return HRESULT_FROM_WIN32(
        ERROR_SET_NOT_FOUND
    );
}

STDMETHODIMP VirtualCameraSource::KsEvent(
    PKSEVENT event,
    ULONG eventLength,
    LPVOID eventData,
    ULONG dataLength,
    ULONG* bytesReturned)
{
    if (bytesReturned)
        *bytesReturned = 0;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (!event)
        return E_POINTER;

    /*
        We currently don't expose any KS events.
    */

    return HRESULT_FROM_WIN32(
        ERROR_SET_NOT_FOUND
    );
}


/*
    IMFSampleAllocatorControl
*/

STDMETHODIMP VirtualCameraSource::SetDefaultAllocator(
    DWORD outputStreamID,
    IUnknown* allocator)
{
    wchar_t message[512]{};

    swprintf_s(
        message,
        ARRAYSIZE(message),
        L"[VirtualCameraSource %p] SetDefaultAllocator ENTER. "
        L"outputStreamID=%lu allocator=%p\n",
        this,
        outputStreamID,
        allocator
    );

    LogToFile(message);
    OutputDebugStringW(message);

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (outputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!allocator)
        return E_POINTER;

    IMFVideoSampleAllocator* suppliedAllocator = nullptr;

    HRESULT hr = allocator->QueryInterface(
        IID_PPV_ARGS(&suppliedAllocator)
    );

    swprintf_s(
        message,
        ARRAYSIZE(message),
        L"[VirtualCameraSource %p] SetDefaultAllocator: "
        L"QI(IMFVideoSampleAllocator) = 0x%08lX result=%p\n",
        this,
        static_cast<unsigned long>(hr),
        suppliedAllocator
    );

    LogToFile(message);
    OutputDebugStringW(message);

    if (FAILED(hr))
        return hr;

    /*
        Diagnostic mode:

        Keep using our own already-initialized DXGI allocator.

        The Frame Server supplied allocator is only queried so that
        we verify the interface is present. We intentionally do not
        replace m_sampleAllocator with it yet.
    */

    if (suppliedAllocator)
    {
        suppliedAllocator->Release();
        suppliedAllocator = nullptr;
    }

    swprintf_s(
        message,
        ARRAYSIZE(message),
        L"[VirtualCameraSource %p] SetDefaultAllocator: "
        L"keeping self-owned allocator=%p owned=%d initialized=%d\n",
        this,
        m_sampleAllocator,
        m_sampleAllocatorOwned ? 1 : 0,
        m_sampleAllocatorInitialized ? 1 : 0
    );

    LogToFile(message);
    OutputDebugStringW(message);

    if (!m_sampleAllocator)
    {
        LogToFile(
            L"[VirtualCameraSource] SetDefaultAllocator: "
            L"self-owned allocator is unavailable\n"
        );

        return MF_E_NOT_INITIALIZED;
    }

    if (!m_sampleAllocatorInitialized)
    {
        LogToFile(
            L"[VirtualCameraSource] SetDefaultAllocator: "
            L"self-owned allocator is not initialized\n"
        );

        return MF_E_NOT_INITIALIZED;
    }

    LogToFile(
        L"[VirtualCameraSource] SetDefaultAllocator EXIT: S_OK\n"
    );

    OutputDebugStringW(
        L"[VirtualCameraSource] SetDefaultAllocator EXIT: S_OK\n"
    );

    return S_OK;
}

STDMETHODIMP VirtualCameraSource::GetAllocatorUsage(
    DWORD outputStreamID,
    DWORD* inputStreamID,
    MFSampleAllocatorUsage* usage)
{
    OutputDebugStringW(
        L"[VirtualCameraSource] GetAllocatorUsage()\n"
    );

    if (!usage)
        return E_POINTER;

    if (m_shutdown)
        return MF_E_SHUTDOWN;

    if (outputStreamID != 0)
    {
        OutputDebugStringW(
            L"[VirtualCameraSource] GetAllocatorUsage: invalid stream ID\n"
        );

        return MF_E_INVALIDSTREAMNUMBER;
    }

    if (inputStreamID)
        *inputStreamID = 0;

    /*
        Our source allocates the samples itself.

        If Windows has supplied an allocator through
        SetDefaultAllocator(), we use it.
    */

    *usage =
        MFSampleAllocatorUsage_UsesProvidedAllocator;

    OutputDebugStringW(
        L"[VirtualCameraSource] GetAllocatorUsage: "
        L"UsesProvidedAllocator\n"
    );

    return S_OK;
}