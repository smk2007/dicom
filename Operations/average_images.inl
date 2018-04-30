#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::AverageImages> 
{
    // Input/output variables
    std::wstring m_inputFolder;
    std::wstring m_outputFile;

    Operation(
        const std::wstring& inputFolder,
        const std::wstring& outputFile) :
        m_inputFolder(inputFolder),
        m_outputFile(outputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);

        struct ImageData
        {
            std::vector<float> Data;
            unsigned Width;
            unsigned Height;
            unsigned Channels;
        };

        Concurrency::ConcurrentQueue<std::shared_ptr<ImageData>> fileQueue(3);

        std::vector<std::wstring> children;
        RETURN_IF_FAILED(GetChildren(m_inputFolder, &children, DCM::FileType::File));

        std::thread t1(
            [](auto resources, auto files, auto queue)
            {
                [&]() -> HRESULT
                {
                    for (auto file : files.get())
                    {
                        auto spImageData = std::make_shared<ImageData>();
                        FAIL_FAST_IF_FAILED(
                            GetBufferFromGrayscaleImage(
                                resources.get(),
                                file.c_str(),
                                &spImageData->Data,
                                &spImageData->Width,
                                &spImageData->Height,
                                &spImageData->Channels
                            ));
                        queue.get().Enqueue(std::move(spImageData));
                    }
                    RETURN_IF_FAILED(queue.get().Finish());
                    return S_OK;
                }();
            }, std::ref(resources), std::ref(children), std::ref(fileQueue));
        
        std::thread t2(
            [](auto resources, auto queue, auto outputFile)
            {
                [&]() -> HRESULT
                {
                    unsigned width = 0;
                    unsigned height = 0;
                    unsigned channels = 0;
                    Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
                    std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs;
                    unsigned slice = 0;

                    bool isDefunct;
                    while (SUCCEEDED(queue.get().IsDefunct(&isDefunct)) && !isDefunct)
                    {
                        std::shared_ptr<ImageData> image;
                        FAIL_FAST_IF_FAILED(queue.get().Dequeue(&image));

                        if (!spOutBuffer || uavs.size() == 0)
                        {
                            width = image->Width;
                            height = image->Height;
                            channels = image->Channels;

                            Log(L"Creating resources for output buffer: (%d, %d)", image->Height, image->Width);
                            std::vector<float> zeroOutBuffer(image->Width * image->Height, 0.f);
                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(
                                sizeof(float) * channels /* size of item */,
                                width * height /* num items */,
                                &zeroOutBuffer[0]/* data */,
                                &spOutBuffer));

                            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
                            FAIL_FAST_IF_FAILED(
                                resources.get().CreateStructuredBufferUAV(
                                    spOutBuffer.Get(),
                                    &spOutBufferUnorderedAccessView));

                            uavs = { spOutBufferUnorderedAccessView };

                            Log(L"Created resources for output buffer.");
                        }

                        FAIL_FAST_IF_FALSE(width == image->Width);
                        FAIL_FAST_IF_FALSE(height == image->Height);
                        FAIL_FAST_IF_FALSE(channels == image->Channels);

                        struct {
                            unsigned Slice;
                            unsigned Unused[3];
                        } constantData =
                        {
                            slice++,
                        };

                        Microsoft::WRL::ComPtr<ID3D11Buffer> spConstantBuffer;
                        RETURN_IF_FAILED(resources.get().CreateConstantBuffer(constantData, &spConstantBuffer));

                        Microsoft::WRL::ComPtr<ID3D11Buffer> spBuffer;
                        RETURN_IF_FAILED(resources.get().CreateStructuredBuffer(
                            sizeof(float) /* size of item */,
                            width * height * channels /* num items */,
                            &(image->Data.at(0)),
                            &spBuffer));

                        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView;
                        RETURN_IF_FAILED(resources.get().CreateStructuredBufferSRV(spBuffer.Get(), &spShaderResourceView));

                        std::vector<ID3D11ShaderResourceView*> sharedResourceViews = { spShaderResourceView.Get() };
                        
                        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
                        RETURN_IF_FAILED(CreateShader(resources, L"Shaders\\average_image.hlsl", &spComputeShader));
                        resources.get().RunComputeShader(spComputeShader.Get(), spConstantBuffer.Get(), 1, &sharedResourceViews[0],
                            uavs, width * height, 1, 1);
                    }

                    RETURN_IF_FAILED(
                        SaveToFile(
                            resources,
                            spOutBuffer.Get(),
                            width,
                            height,
                            sizeof(float) * channels,
                            outputFile.c_str()));
                    return S_OK;
                }();
            }, std::ref(resources), std::ref(fileQueue), m_outputFile);

        t1.join();
        t2.join();

        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::AverageImages>() { Log(L"[OperationType::AverageImages]"); }

} // Operations
} // DCM