#pragma once

namespace Application
{
namespace Infrastructure
{

class DeviceResources
{
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dDeviceContext;
    Microsoft::WRL::ComPtr<IWICImagingFactory2>	m_wicFactory;

public:

    DeviceResources()
    {
        CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&m_wicFactory)
        );

        CreateComputeDevice();
    }

    void RunComputeShader(
        ID3D11ComputeShader* pComputeShader,
        ID3D11Buffer* pConstantBuffer,
        UINT nShaderResourceViews,
        ID3D11ShaderResourceView** pShaderResourceViews,
        std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs,
        UINT X,
        UINT Y,
        UINT Z)
    {
        // Setup shader
        m_d3dDeviceContext->CSSetShader(pComputeShader, nullptr, 0);
        if (nShaderResourceViews > 0)
        {
            m_d3dDeviceContext->CSSetShaderResources(0, nShaderResourceViews, pShaderResourceViews);
        }
        if (uavs.size() > 0)
        {
            std::vector<ID3D11UnorderedAccessView*> _uavs(uavs.size());
            std::transform(std::begin(uavs), std::end(uavs), std::begin(_uavs), [](auto uav) -> auto { return uav.Get(); });
            m_d3dDeviceContext->CSSetUnorderedAccessViews(0, static_cast<unsigned>(_uavs.size()), &_uavs[0], nullptr);
        }
        if (pConstantBuffer)
        {
            m_d3dDeviceContext->CSSetConstantBuffers(0, 1, &pConstantBuffer);
        }

        // Run
        m_d3dDeviceContext->Dispatch(X, Y, Z);

        // Revert
        m_d3dDeviceContext->CSSetShader(nullptr, nullptr, 0);
        if (nShaderResourceViews > 0)
        {
            std::vector<ID3D11ShaderResourceView*> srvNullptr(nShaderResourceViews, nullptr);
            m_d3dDeviceContext->CSSetShaderResources(0, nShaderResourceViews, &srvNullptr[0]);
        }
        if (uavs.size() > 0)
        {
            std::vector<ID3D11UnorderedAccessView*> uavNullptr(uavs.size(), nullptr);
            m_d3dDeviceContext->CSSetUnorderedAccessViews(0, static_cast<unsigned>(uavs.size()), &uavNullptr[0], nullptr);
        }
        if (pConstantBuffer)
        {
            std::vector<ID3D11Buffer*> csBufferNullptr(1, nullptr);
            m_d3dDeviceContext->CSSetConstantBuffers(0, 1, &csBufferNullptr[0]);
        }
    }

    HRESULT Map(ID3D11Buffer* pBuffer, D3D11_MAPPED_SUBRESOURCE* mappedResource)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pBuffer);
        RETURN_HR_IF_NULL(E_POINTER, mappedResource);
        RETURN_IF_FAILED(m_d3dDeviceContext->Map(pBuffer, 0, D3D11_MAP_READ, 0, mappedResource));
        return S_OK;
    }

    HRESULT Unmap(ID3D11Buffer* pBuffer)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pBuffer);
        m_d3dDeviceContext->Unmap(pBuffer, 0);
        return S_OK;
    }

    HRESULT GetBufferOnCPU(ID3D11Buffer* pBuffer, ID3D11Buffer** pCPUBuffer)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pBuffer);
        RETURN_HR_IF_NULL(E_POINTER, pCPUBuffer);

        Microsoft::WRL::ComPtr<ID3D11Buffer> spDebug;

        D3D11_BUFFER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        pBuffer->GetDesc(&desc);
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        RETURN_IF_FAILED(m_d3dDevice->CreateBuffer(&desc, nullptr, &spDebug));
#if defined(_DEBUG) || defined(PROFILE)
        RETURN_IF_FAILED(spDebug->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Debug") - 1, "Debug"));
#endif
        m_d3dDeviceContext->CopyResource(spDebug.Get(), pBuffer);

        *pCPUBuffer = spDebug.Detach();
        return S_OK;
    }

    HRESULT CreateComputeShader(LPCWSTR pSrcFile, LPCSTR pFunctionName, ID3D11ComputeShader** ppShaderOut)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pSrcFile);
        RETURN_HR_IF_NULL(E_INVALIDARG, pFunctionName);
        RETURN_HR_IF_NULL(E_POINTER, ppShaderOut);

        DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
        // Setting this flag improves the shader debugging experience, but still allows 
        // the shaders to be optimized and to run exactly the way they will run in 
        // the release configuration of this program.
        dwShaderFlags |= D3DCOMPILE_DEBUG;

        // Disable optimizations to further improve shader debugging
        dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        const D3D_SHADER_MACRO defines[] =
        {
            "USE_STRUCTURED_BUFFERS", "1",
            nullptr, nullptr
        };

        // We generally prefer to use the higher CS shader profile when possible as CS 5.0 is better performance on 11-class hardware
        LPCSTR pProfile = (m_d3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";

        Microsoft::WRL::ComPtr<ID3DBlob> spErrorBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> spBlob;

#if D3D_COMPILER_VERSION >= 46
        auto hr = D3DCompileFromFile(pSrcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, pFunctionName, pProfile,
            dwShaderFlags, 0, &spBlob, &spErrorBlob);
#else
        auto hr = D3DX11CompileFromFile(pSrcFile, defines, nullptr, pFunctionName, pProfile,
            dwShaderFlags, 0, nullptr, &spBlob, &spErrorBlob, nullptr);
#endif

        if (FAILED(hr) && spErrorBlob)
        {
            OutputDebugStringA((char*)spErrorBlob->GetBufferPointer());
        }
        RETURN_IF_FAILED(hr);

        RETURN_IF_FAILED(m_d3dDevice->CreateComputeShader(spBlob->GetBufferPointer(), spBlob->GetBufferSize(), nullptr, ppShaderOut));

#if defined(_DEBUG) || defined(PROFILE)
        (*ppShaderOut)->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(pFunctionName), pFunctionName);
#endif

        return S_OK;
    }


    template <typename T>
    HRESULT CreateConstantBuffer(T& initData, ID3D11Buffer** ppBufOut)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppBufOut);
        *ppBufOut = nullptr;

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = sizeof(T);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA subresourceData;
        subresourceData.pSysMem = &initData;
        subresourceData.SysMemPitch = 0;
        subresourceData.SysMemSlicePitch = 0;
        return m_d3dDevice->CreateBuffer(&desc, &subresourceData, ppBufOut);
    }

    HRESULT CreateStructuredBuffer(UINT uElementSize, UINT uCount, void* pInitData, ID3D11Buffer** ppBufOut)
    {
        *ppBufOut = nullptr;

        D3D11_BUFFER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        desc.ByteWidth = uElementSize * uCount;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = uElementSize;

        if (pInitData)
        {
            D3D11_SUBRESOURCE_DATA InitData;
            InitData.pSysMem = pInitData;
            return m_d3dDevice->CreateBuffer(&desc, &InitData, ppBufOut);
        }
        else
            return m_d3dDevice->CreateBuffer(&desc, nullptr, ppBufOut);
    }

    HRESULT CreateStructuredBufferSRV(ID3D11Buffer* pBuffer, ID3D11ShaderResourceView** ppSRVOut)
    {
        D3D11_BUFFER_DESC descBuf;
        ZeroMemory(&descBuf, sizeof(descBuf));
        pBuffer->GetDesc(&descBuf);

        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        desc.BufferEx.FirstElement = 0;

        RETURN_HR_IF_FALSE(E_INVALIDARG, (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0);
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.BufferEx.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
        
        return m_d3dDevice->CreateShaderResourceView(pBuffer, &desc, ppSRVOut);
    }

    HRESULT CreateStructuredBufferUAV(ID3D11Buffer* pBuffer, ID3D11UnorderedAccessView** ppUAVOut)
    {
        D3D11_BUFFER_DESC descBuf;
        ZeroMemory(&descBuf, sizeof(descBuf));
        pBuffer->GetDesc(&descBuf);

        D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;

        RETURN_HR_IF_FALSE(E_INVALIDARG, (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0);
        desc.Format = DXGI_FORMAT_UNKNOWN;      // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
        desc.Buffer.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
        
        return m_d3dDevice->CreateUnorderedAccessView(pBuffer, &desc, ppUAVOut);
    }

    //--------------------------------------------------------------------------------------
    // Create the D3D device and device context suitable for running Compute Shaders(CS)
    //--------------------------------------------------------------------------------------
    HRESULT CreateComputeDevice()
    {
        HRESULT hr = S_OK;

        UINT uCreationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
        uCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL flOut;
        static const D3D_FEATURE_LEVEL flvl[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

        bool bNeedRefDevice = false;
        hr = D3D11CreateDevice(nullptr,                        // Use default graphics card
            D3D_DRIVER_TYPE_HARDWARE,    // Try to create a hardware accelerated device
            nullptr,                        // Do not use external software rasterizer module
            uCreationFlags,              // Device creation flags
            flvl,
            sizeof(flvl) / sizeof(D3D_FEATURE_LEVEL),
            D3D11_SDK_VERSION,           // SDK version
            &m_d3dDevice,                 // Device out
            &flOut,                      // Actual feature level created
            &m_d3dDeviceContext);              // Context out

        if (SUCCEEDED(hr))
        {
            // A hardware accelerated device has been created, so check for Compute Shader support

            // If we have a device >= D3D_FEATURE_LEVEL_11_0 created, full CS5.0 support is guaranteed, no need for further checks
            if (flOut < D3D_FEATURE_LEVEL_11_0)
            {
                // Otherwise, we need further check whether this device support CS4.x (Compute on 10)
                D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
                m_d3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
                if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
                {
                    bNeedRefDevice = true;
                    printf("No hardware Compute Shader capable device found, trying to create ref device.\n");
                }
            }
        }

        if (FAILED(hr) || bNeedRefDevice)
        {
            // Either because of failure on creating a hardware device or hardware lacking CS capability, we create a ref device here
            m_d3dDevice = nullptr;
            m_d3dDeviceContext = nullptr;

            hr = D3D11CreateDevice(nullptr,                        // Use default graphics card
                D3D_DRIVER_TYPE_REFERENCE,   // Try to create a hardware accelerated device
                nullptr,                        // Do not use external software rasterizer module
                uCreationFlags,              // Device creation flags
                flvl,
                sizeof(flvl) / sizeof(D3D_FEATURE_LEVEL),
                D3D11_SDK_VERSION,                      // SDK version
                &m_d3dDevice,                 // Device out
                &flOut,                      // Actual feature level created
                &m_d3dDeviceContext);              // Context out


            if (FAILED(hr))
            {
                m_d3dDevice = nullptr;
                m_d3dDeviceContext = nullptr;

                // If the initialization fails, fall back to the WARP device.
                // For more information on WARP, see: 
                // https://go.microsoft.com/fwlink/?LinkId=286690
                FAIL_FAST_IF_FAILED(
                    D3D11CreateDevice(
                        nullptr,
                        D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                        0,
                        uCreationFlags,
                        flvl,
                        sizeof(flvl) / sizeof(D3D_FEATURE_LEVEL),
                        D3D11_SDK_VERSION,           // SDK version
                        &m_d3dDevice,                 // Device out
                        &flOut,                      // Actual feature level created
                        &m_d3dDeviceContext
                    )
                );
            }

            if (FAILED(hr))
            {
                printf("Reference rasterizer device create failure\n");
                return hr;
            }
        }

        return hr;
    }

    ID3D11Device*                GetD2DDevice()         const { return m_d3dDevice.Get(); }
    IWICImagingFactory2*         GetWicImagingFactory() const { return m_wicFactory.Get(); }
};

} // Infrastructure
} // Application