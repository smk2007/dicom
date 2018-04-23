#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::SignalToNoise> 
{
    std::wstring m_signalFile;
    std::wstring m_noiseFile;
    std::wstring m_outputFile;

    Operation(
        const std::wstring& signalFile,
        const std::wstring& noiseFile,
        const std::wstring& outputFile) :
        m_signalFile(signalFile),
        m_noiseFile(noiseFile),
        m_outputFile(outputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);

        // Create the shader
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
        RETURN_IF_FAILED(resources.CreateComputeShader(L"Shaders\\divide_images.hlsl", "CSMain", &spComputeShader));

        struct {
            float Factor;
            unsigned UNUSED[3];
        } constantData =
        {
            1.0f
        };

        Microsoft::WRL::ComPtr<ID3D11Buffer> spConstantBuffer;
        RETURN_IF_FAILED(resources.CreateConstantBuffer(constantData, &spConstantBuffer));

        // Get signal file
        Microsoft::WRL::ComPtr<ID3D11Buffer> spSignalBuffer;
        unsigned signalWidth;
        unsigned signalHeight;
        {
            unsigned signalChannels;
            std::vector<float> data;

            RETURN_IF_FAILED(GetBufferFromGrayscaleImage(resources, m_signalFile.c_str(), &data, &signalWidth, &signalHeight, &signalChannels));
            RETURN_HR_IF_FALSE(E_FAIL, signalChannels == 1);

            // Create signal input buffer
            RETURN_IF_FAILED(resources.CreateStructuredBuffer(
                sizeof(float) /* size of item */,
                signalWidth * signalHeight * signalChannels /* num items */,
                &data.at(0),
                &spSignalBuffer));
        }

        // Create signal resource view
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spSignalShaderResourceView;
        RETURN_IF_FAILED(resources.CreateStructuredBufferSRV(spSignalBuffer.Get(), &spSignalShaderResourceView));

        // Get noise file
        Microsoft::WRL::ComPtr<ID3D11Buffer> spNoiseBuffer;
        unsigned noiseWidth;
        unsigned noiseHeight;
        {
            unsigned noiseChannels;
            std::vector<float> data;

            RETURN_IF_FAILED(GetBufferFromGrayscaleImage(resources, m_signalFile.c_str(), &data, &noiseWidth, &noiseHeight, &noiseChannels));
            RETURN_HR_IF_FALSE(E_FAIL, noiseChannels == 1);
            RETURN_HR_IF_FALSE(E_FAIL, noiseHeight == signalHeight);
            RETURN_HR_IF_FALSE(E_FAIL, noiseWidth == signalWidth);

            // Create signal input buffer
            RETURN_IF_FAILED(resources.CreateStructuredBuffer(
                sizeof(float) /* size of item */,
                noiseWidth * noiseHeight * noiseChannels /* num items */,
                &data.at(0),
                &spNoiseBuffer));
        }

        // Create signal resource view
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spNoiseShaderResourceView;
        RETURN_IF_FAILED(resources.CreateStructuredBufferSRV(spNoiseBuffer.Get(), &spNoiseShaderResourceView));

        std::vector<ID3D11ShaderResourceView*> sharedResourceViews =
            { spSignalShaderResourceView.Get(), spNoiseShaderResourceView.Get() };

        // Create out buffer
        Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
        RETURN_IF_FAILED(
            resources.CreateStructuredBuffer(
                sizeof(float) /* size of item */,
                signalWidth * signalHeight /* num items */,
                nullptr,
                &spOutBuffer));

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
        RETURN_IF_FAILED(resources.CreateStructuredBufferUAV(spOutBuffer.Get(), &spOutBufferUnorderedAccessView));

        std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs = { spOutBufferUnorderedAccessView.Get() };

        resources.RunComputeShader(spComputeShader.Get(), spConstantBuffer.Get(), 2, &sharedResourceViews.at(0), uavs, signalWidth * signalHeight, 1 , 1);

        // Save output to file
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppGrayFloat;
        RETURN_IF_FAILED(
            SaveToFile(
                resources,
                spOutBuffer.Get(),
                signalWidth,
                signalHeight,
                sizeof(float),
                format,
                m_outputFile.c_str()));

        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::SignalToNoise>() { Log(L"[OperationType::SignalToNoise]"); }

} // Operations
} // DCM