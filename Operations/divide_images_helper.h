#pragma once

namespace DCM
{
namespace Operations
{

inline HRESULT DivideImages(
    Application::Infrastructure::DeviceResources& resources,
    const std::wstring& dividendFile,
    const std::wstring& divisorFile, 
    const std::wstring& m_outputFile,
    float factor = 1.0f)
{
    // Create the shader
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
    RETURN_IF_FAILED(resources.CreateComputeShader(L"Shaders\\divide_images.hlsl", "CSMain", &spComputeShader));

    struct {
        float Factor;
        unsigned UNUSED[3];
    } constantData = { factor };

    Microsoft::WRL::ComPtr<ID3D11Buffer> spConstantBuffer;
    RETURN_IF_FAILED(resources.CreateConstantBuffer(constantData, &spConstantBuffer));

    // Get dividend file
    Microsoft::WRL::ComPtr<ID3D11Buffer> spDividendBuffer;
    unsigned dividendWidth;
    unsigned dividendHeight;
    {
        unsigned dividendChannels;
        std::vector<float> data;

        RETURN_IF_FAILED(GetBufferFromGrayscaleImage(resources, dividendFile.c_str(), &data, &dividendWidth, &dividendHeight, &dividendChannels));
        RETURN_HR_IF_FALSE(E_FAIL, dividendChannels == 1);

        // Create dividend input buffer
        RETURN_IF_FAILED(resources.CreateStructuredBuffer(
            sizeof(float) /* size of item */,
            dividendWidth * dividendHeight * dividendChannels /* num items */,
            &data.at(0),
            &spDividendBuffer));
    }

    // Create dividend resource view
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spDividendShaderResourceView;
    RETURN_IF_FAILED(resources.CreateStructuredBufferSRV(spDividendBuffer.Get(), &spDividendShaderResourceView));

    // Get divisor file
    Microsoft::WRL::ComPtr<ID3D11Buffer> spDivisorBuffer;
    unsigned divisorWidth;
    unsigned divisorHeight;
    {
        unsigned divisorChannels;
        std::vector<float> data;

        RETURN_IF_FAILED(GetBufferFromGrayscaleImage(resources, divisorFile.c_str(), &data, &divisorWidth, &divisorHeight, &divisorChannels));
        RETURN_HR_IF_FALSE(E_FAIL, divisorChannels == 1);
        RETURN_HR_IF_FALSE(E_FAIL, divisorHeight == dividendHeight);
        RETURN_HR_IF_FALSE(E_FAIL, divisorWidth == dividendWidth);

        // Create dividend input buffer
        RETURN_IF_FAILED(resources.CreateStructuredBuffer(
            sizeof(float) /* size of item */,
            divisorWidth * divisorHeight * divisorChannels /* num items */,
            &data.at(0),
            &spDivisorBuffer));
    }

    // Create dividend resource view
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spDivisorShaderResourceView;
    RETURN_IF_FAILED(resources.CreateStructuredBufferSRV(spDivisorBuffer.Get(), &spDivisorShaderResourceView));

    std::vector<ID3D11ShaderResourceView*> sharedResourceViews =
    { spDividendShaderResourceView.Get(), spDivisorShaderResourceView.Get() };

    // Create out buffer
    Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
    RETURN_IF_FAILED(
        resources.CreateStructuredBuffer(
            sizeof(float) /* size of item */,
            dividendWidth * dividendHeight /* num items */,
            nullptr,
            &spOutBuffer));

    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
    RETURN_IF_FAILED(resources.CreateStructuredBufferUAV(spOutBuffer.Get(), &spOutBufferUnorderedAccessView));

    std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs = { spOutBufferUnorderedAccessView.Get() };

    resources.RunComputeShader(spComputeShader.Get(), spConstantBuffer.Get(), 2, &sharedResourceViews.at(0), uavs, dividendWidth * dividendHeight, 1, 1);

    // Save output to file
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppGrayFloat;
    RETURN_IF_FAILED(
        SaveToFile(
            resources,
            spOutBuffer.Get(),
            dividendWidth,
            dividendHeight,
            sizeof(float),
            format,
            m_outputFile.c_str()));

    return S_OK;
}

} // Operations
} // DCM
