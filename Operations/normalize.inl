#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::Normalize> 
{
private:
    std::wstring m_inputFile;
    std::wstring m_outputFile;

public:
    Operation(
        const std::wstring& inputFile,
        const std::wstring& outputFile) : 
            m_inputFile(inputFile),
            m_outputFile(outputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);

        // Create the shader
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
        RETURN_IF_FAILED(resources.CreateComputeShader(L"Shaders\\normalize_image.hlsl", "CSMain", &spComputeShader));


        Microsoft::WRL::ComPtr<IWICBitmapDecoder> spDecoder;
        RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateDecoderFromFilename(
            m_inputFile.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &spDecoder
        ));

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> spDecodedFrame;
        RETURN_IF_FAILED(spDecoder->GetFrame(0, &spDecodedFrame));

        Microsoft::WRL::ComPtr<IWICFormatConverter> spConverter;
        RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateFormatConverter(&spConverter));

        RETURN_IF_FAILED(spConverter->Initialize(
            spDecodedFrame.Get(),
            GUID_WICPixelFormat32bppGrayFloat,
            WICBitmapDitherTypeNone,
            nullptr,
            0.f,
            WICBitmapPaletteTypeCustom));


        unsigned width, height;
        RETURN_IF_FAILED(spConverter->GetSize(&width, &height));

        IID clsid;
        spConverter->GetPixelFormat(&clsid);
        Microsoft::WRL::ComPtr<IWICComponentInfo> spComponentInfo;
        RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateComponentInfo(clsid, &spComponentInfo));

        Microsoft::WRL::ComPtr<IWICPixelFormatInfo> spWICPixelFormatInfo;
        RETURN_IF_FAILED(spComponentInfo->QueryInterface(IID_PPV_ARGS(&spWICPixelFormatInfo)));
        
        unsigned bpp;
        RETURN_IF_FAILED(spWICPixelFormatInfo->GetBitsPerPixel(&bpp));
        unsigned bytesPerPixel = bpp / 8;
        RETURN_HR_IF_FALSE(E_FAIL, bytesPerPixel == sizeof(float));

        unsigned nChannels;
        RETURN_IF_FAILED(spWICPixelFormatInfo->GetChannelCount(&nChannels));
        RETURN_HR_IF_FALSE(E_FAIL, nChannels == 1);

        unsigned stride = bytesPerPixel * width;
        unsigned bufferSize = stride * height;
        std::vector<float> data(nChannels * width * height);

        RETURN_IF_FAILED(spConverter->CopyPixels(nullptr, stride, bufferSize, reinterpret_cast<unsigned char*>(&data.at(0))));

        auto min = std::min_element(std::begin(data), std::end(data));
        auto max = std::max_element(std::begin(data), std::end(data));

        struct {
            float minimum;
            float maximum;
            double unused;
        } constantData =
        {
            *min,
            *max
        };

        Microsoft::WRL::ComPtr<ID3D11Buffer> spConstantBuffer;
        RETURN_IF_FAILED(resources.CreateConstantBuffer(constantData, &spConstantBuffer));

        Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
        RETURN_IF_FAILED(
            resources.CreateStructuredBuffer(
            sizeof(float) /* size of item */,
            width * height * nChannels /* num items */,
            &data.at(0),
            &spOutBuffer));

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutUnorderedAccessView;
        RETURN_IF_FAILED(resources.CreateStructuredBufferUAV(spOutBuffer.Get(), &spOutUnorderedAccessView));

        resources.RunComputeShader(spComputeShader.Get(), spConstantBuffer.Get(), 0, nullptr,
            spOutUnorderedAccessView.Get(), width * height * nChannels, 1, 1);

        WICPixelFormatGUID format = GUID_WICPixelFormat32bppGrayFloat;
        RETURN_IF_FAILED(SaveToFile(resources, spOutBuffer.Get(), width, height, sizeof(float), format, m_outputFile.c_str()));

        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::Normalize>() { Log(L"[OperationType::Normalize]"); }

} // Operations
} // DCM