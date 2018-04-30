#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::ConvertToFloat>
{
private:
    std::wstring m_inputFile;
    unsigned m_rows;
    unsigned m_columns;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_spOutBuffer;
public:

    ID3D11Buffer * GetBuffer() { return m_spOutBuffer.Get(); };
    unsigned GetRows(){ return m_rows; };
    unsigned GetColumns(){ return m_columns; };

    Operation(
        const std::wstring& inputFile) :
        m_inputFile(inputFile)
    {}

    HRESULT ProcessDicomFile(Application::Infrastructure::DeviceResources& resources)
    {
        // Get the full file
        std::shared_ptr<DicomFile> file;
        RETURN_IF_FAILED(MakeDicomImageFile(m_inputFile, &file));

        m_rows = Property<ImageProperty::Rows>::SafeGet(file);
        m_columns = Property<ImageProperty::Columns>::SafeGet(file);

        // Get data as structured buffer
        // Because structured buffers require a minumum size of 4 bytes per element,
        // 2 pixels are packed together.
        auto data = Property<ImageProperty::PixelData>::SafeGet(file);
        Microsoft::WRL::ComPtr<ID3D11Buffer> spBuffer;
        FAIL_FAST_IF_FAILED(resources.CreateStructuredBuffer(
            sizeof(short) * 2 /* size of item */,
            static_cast<unsigned>(data->size() / 4) /* num items */,
            &data->at(0) /* data */,
            &spBuffer));

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView;
        FAIL_FAST_IF_FAILED(resources.CreateStructuredBufferSRV(spBuffer.Get(), &spShaderResourceView));

        std::vector<ID3D11ShaderResourceView*> sharedResourceViews = { spShaderResourceView.Get() };

        Log(L"Creating resources for output buffer: (%d, %d)", m_rows, m_columns);
        FAIL_FAST_IF_FAILED(resources.CreateStructuredBuffer(
            sizeof(float) /* size of item */,
            m_rows * m_columns /* num items */,
            nullptr/* data */,
            &m_spOutBuffer));

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
        FAIL_FAST_IF_FAILED(
            resources.CreateStructuredBufferUAV(
                m_spOutBuffer.Get(),
                &spOutBufferUnorderedAccessView));

        std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs = { spOutBufferUnorderedAccessView };

        struct {
            unsigned Column;
            unsigned UNUSED[3];
        } constantAddData = { m_columns };

        Microsoft::WRL::ComPtr<ID3D11Buffer> spAddConstantBuffer;
        FAIL_FAST_IF_FAILED(resources.CreateConstantBuffer(constantAddData, &spAddConstantBuffer));


        // Create the shader
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
        RETURN_IF_FAILED(CreateShader(resources, L"Shaders\\convert_to_float.hlsl", &spComputeShader));

        resources.RunComputeShader(spComputeShader.Get(), spAddConstantBuffer.Get(),
            1, &sharedResourceViews.at(0), uavs, m_rows, m_columns, 1);

        return S_OK;
    }

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        if (m_inputFile.rfind(L".IMA") + 4 == m_inputFile.size())
        {
            return ProcessDicomFile(resources);
        }

        std::vector<float> data;
        unsigned nChannels;
        RETURN_IF_FAILED(GetBufferFromGrayscaleImage(
            resources, m_inputFile.c_str(), &data, &m_columns, &m_rows, &nChannels));
        RETURN_HR_IF_FALSE(E_FAIL, nChannels == 1);

        return resources.CreateStructuredBuffer(
            sizeof(float) /* size of item */,
            m_rows * m_columns * nChannels /* num items */,
            &data.at(0),
            &m_spOutBuffer);
    }
};

template <> void inline LogOperation<OperationType::ConvertToFloat>() { Log(L"[OperationType::ConvertToFloat]"); }

} // Operations
} // DCM