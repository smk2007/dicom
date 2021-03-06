#pragma once

#undef max
#undef min

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::Normalize> 
{
private:
    std::wstring m_inputFile;
    std::wstring m_outputFile;

    float m_min;
    float m_max;

public:
    Operation(
        const std::wstring& inputFile,
        const std::wstring& outputFile) : 
            m_inputFile(inputFile),
            m_outputFile(outputFile),
            m_min(std::numeric_limits<float>::min()),
            m_max(std::numeric_limits<float>::min())
    {}

    Operation(
        const std::wstring& inputFile,
        const std::wstring& outputFile,
        float min,
        float max) :
        m_inputFile(inputFile),
        m_outputFile(outputFile),
        m_min(min),
        m_max(max)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);

        std::vector<float> data;
        unsigned width;
        unsigned height;
        unsigned nChannels;
        RETURN_IF_FAILED(GetBufferFromGrayscaleImage(resources, m_inputFile.c_str(), &data, &width, &height, &nChannels));
        RETURN_HR_IF_FALSE(E_FAIL, nChannels == 1);

        auto min = 
            (m_min == std::numeric_limits<float>::min()) ?
                *std::min_element(std::begin(data), std::end(data)) : 
                m_min;
        auto max =
            m_max == std::numeric_limits<float>::min() ? 
                *std::max_element(std::begin(data), std::end(data)) :
                m_max;

        struct {
            float minimum;
            float maximum;
            double unused;
        } constantData =
        {
            min,
            max
        };

        Microsoft::WRL::ComPtr<ID3D11Buffer> spConstantBuffer;
        RETURN_IF_FAILED(resources.CreateConstantBuffer(constantData, &spConstantBuffer));

        Microsoft::WRL::ComPtr<ID3D11Buffer> spBuffer;
        RETURN_IF_FAILED(resources.CreateStructuredBuffer(
            sizeof(float) /* size of item */,
            width * height * nChannels /* num items */,
            &data.at(0),
            &spBuffer));

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView;
        RETURN_IF_FAILED(resources.CreateStructuredBufferSRV(spBuffer.Get(), &spShaderResourceView));

        std::vector<ID3D11ShaderResourceView*> sharedResourceViews = { spShaderResourceView.Get() };

        Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
        RETURN_IF_FAILED(
            resources.CreateStructuredBuffer(
                sizeof(unsigned short) * 4 /* size of item */,
                width * height /* num items */,
                nullptr,
                &spOutBuffer));

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
        RETURN_IF_FAILED(resources.CreateStructuredBufferUAV(spOutBuffer.Get(), &spOutBufferUnorderedAccessView));

        std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs = { spOutBufferUnorderedAccessView.Get() };

        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
        RETURN_IF_FAILED(CreateShader(resources, L"Shaders\\normalize_image.hlsl", &spComputeShader));
        resources.RunComputeShader(spComputeShader.Get(), spConstantBuffer.Get(),
            1, &sharedResourceViews.at(0), uavs, width * height, 1, 1);

        WICPixelFormatGUID format = GUID_WICPixelFormat64bppRGBA;
        RETURN_IF_FAILED(SaveToFile(resources, spOutBuffer.Get(), width, height,
            sizeof(unsigned short) * 4, format, m_outputFile.c_str()));

        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::Normalize>() { Log(L"[OperationType::Normalize]"); }

} // Operations
} // DCM