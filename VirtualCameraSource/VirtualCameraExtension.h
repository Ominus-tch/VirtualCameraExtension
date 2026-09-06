#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>

#include <mfidl.h>
#include <mfobjects.h>

#include <ks.h>
#include <ksproxy.h>

#include <atomic>

#include "VirtualCameraFormat.h"

class VirtualCameraStream;

class VirtualCameraSource final
    : public IMFMediaSourceEx
    , public IMFGetService
    , public IKsControl
    , public IMFSampleAllocatorControl
{
public:
    VirtualCameraSource(
        IMFAttributes* activationAttributes,
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

    // IMFMediaSource
    STDMETHODIMP GetCharacteristics(
        DWORD* characteristics
    ) override;

    STDMETHODIMP CreatePresentationDescriptor(
        IMFPresentationDescriptor** presentationDescriptor
    ) override;

    STDMETHODIMP Start(
        IMFPresentationDescriptor* presentationDescriptor,
        const GUID* timeFormat,
        const PROPVARIANT* startPosition
    ) override;

    STDMETHODIMP Stop() override;

    STDMETHODIMP Pause() override;

    STDMETHODIMP Shutdown() override;

    // IMFMediaSourceEx
    STDMETHODIMP GetSourceAttributes(
        IMFAttributes** attributes
    ) override;

    STDMETHODIMP GetStreamAttributes(
        DWORD streamIdentifier,
        IMFAttributes** attributes
    ) override;

    STDMETHODIMP SetD3DManager(
        IUnknown* manager
    ) override;

    // IMFGetService
    STDMETHODIMP GetService(
        REFGUID guidService,
        REFIID riid,
        LPVOID* ppvObject
    ) override;

    // IKsControl
    STDMETHODIMP KsProperty(
        PKSPROPERTY property,
        ULONG propertyLength,
        LPVOID propertyData,
        ULONG dataLength,
        ULONG* bytesReturned
    ) override;

    STDMETHODIMP KsMethod(
        PKSMETHOD method,
        ULONG methodLength,
        LPVOID methodData,
        ULONG dataLength,
        ULONG* bytesReturned
    ) override;

    STDMETHODIMP KsEvent(
        PKSEVENT event,
        ULONG eventLength,
        LPVOID eventData,
        ULONG dataLength,
        ULONG* bytesReturned
    ) override;

    // IMFSampleAllocatorControl
    STDMETHODIMP SetDefaultAllocator(
        DWORD outputStreamID,
        IUnknown* allocator
    ) override;

    STDMETHODIMP GetAllocatorUsage(
        DWORD outputStreamID,
        DWORD* inputStreamID,
        MFSampleAllocatorUsage* usage
    ) override;

    HRESULT AllocateSample(
        IMFSample** sample
    );

    HRESULT CopySharedFrameToTexture(
        ID3D11Texture2D* destination
    );

private:
    HRESULT InitializeD3D();

    HRESULT InitializeScalingResources(
        const D3D11_TEXTURE2D_DESC& sharedDescription
    );

    void ReleaseScalingResources();

    ~VirtualCameraSource();

    std::atomic<ULONG> m_refCount{ 1 };

    VirtualCameraFormat m_format{};

    IMFMediaEventQueue* m_eventQueue = nullptr;
    IMFPresentationDescriptor* m_presentationDescriptor = nullptr;

    VirtualCameraStream* m_stream = nullptr;

    IMFAttributes* m_sourceAttributes = nullptr;

    IMFVideoSampleAllocator* m_sampleAllocator = nullptr;
    bool m_sampleAllocatorOwned = false;
    bool m_sampleAllocatorInitialized = false;

    // Self-owned D3D11 device and Media Foundation DXGI manager.
    IMFDXGIDeviceManager* m_dxgiManager = nullptr;
    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;

    // Shared PianoVisualizer frame.
    ID3D11Texture2D* m_sharedTexture = nullptr;
    IDXGIKeyedMutex* m_sharedMutex = nullptr;

    // GPU scaling resources.
    ID3D11Texture2D* m_scaleTexture = nullptr;
    ID3D11ShaderResourceView* m_scaleSRV = nullptr;
    ID3D11RenderTargetView* m_scaleRTV = nullptr;

    ID3D11VertexShader* m_scaleVertexShader = nullptr;
    ID3D11PixelShader* m_scalePixelShader = nullptr;
    ID3D11SamplerState* m_scaleSampler = nullptr;

    UINT m_scaleTextureWidth = 0;
    UINT m_scaleTextureHeight = 0;
    DXGI_FORMAT m_scaleTextureFormat = DXGI_FORMAT_UNKNOWN;

    bool m_shutdown = false;
};
