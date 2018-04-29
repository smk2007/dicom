#pragma once


namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::VoxelizeStdDev>
{
    // Voxel dimensions
    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    // Input/output variables
    std::wstring m_inputFolder;
    std::wstring m_outputFile;

    Operation(
        const std::wstring& inputFolder,
        const std::wstring& outputFile,
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters) :
        m_inputFolder(inputFolder),
        m_outputFile(outputFile),
        m_xInMillimeters(xInMillimeters),
        m_yInMillimeters(yInMillimeters),
        m_zInMillimeters(zInMillimeters)
    {}

    static std::wstring GetShaderFromPath(const wchar_t* path)
    {
        wchar_t pwzFileName[MAX_PATH + 1];
        FAIL_FAST_IF_TRUE(0 == GetModuleFileName(NULL, pwzFileName, MAX_PATH + 1));
        std::wstring shaderPath(pwzFileName);
        shaderPath.erase(shaderPath.begin() + shaderPath.find_last_of(L'\\') + 1, shaderPath.end());
        shaderPath += path;
        return shaderPath;
    }

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        std::vector<std::shared_ptr<DicomFile>> metadataFiles;
        RETURN_IF_FAILED(GetMetadataFiles(m_inputFolder, &metadataFiles));
        RETURN_IF_FAILED(SortFilesInScene(&metadataFiles));
        auto nFiles = static_cast<unsigned>(metadataFiles.size());

        Concurrency::ConcurrentQueue<std::shared_ptr<DicomFile>> fileQueue(100);

        std::thread t1([](auto metadataFiles, auto fileQueue)
        {
            [&]()->HRESULT
            {
                for (auto file : metadataFiles.get())
                {
                    // Get the full file
                    std::shared_ptr<DicomFile> fullFile;
                    FAIL_FAST_IF_FAILED(MakeDicomImageFile(file->SafeGetFilename(), &fullFile));
                     RETURN_IF_FAILED(fileQueue.get().Enqueue(std::move(fullFile)));
                }

                fileQueue.get().Finish();
                return S_OK;
            }();
        }, std::ref(metadataFiles), std::ref(fileQueue));

        std::thread t2(
            [](auto resources,
                auto fileQueue,
                auto nFiles,
                auto voxelWidthInMillimeters,
                auto voxelHeightInMillimeters,
                auto voxelDepthInMillimeters,
                auto outputFile)
            {
                [&]()->HRESULT
                {
                    // Create the shader
                    Microsoft::WRL::ComPtr<ID3D11ComputeShader> spMeansComputeShader;
                    RETURN_IF_FAILED(resources.get().CreateComputeShader(
                        GetShaderFromPath(L"Shaders\\voxelize_mean.hlsl").c_str(),
                        "CSMain", &spMeansComputeShader));

                    Microsoft::WRL::ComPtr<ID3D11Buffer> spMeanBuffer;
                    Microsoft::WRL::ComPtr<ID3D11Buffer> spMeanSquaredBuffer;
                    Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBufferCounts;

                    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spMeanBufferUAV;
                    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spMeanSquaredBufferUAV;
                    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferCountsUAV;

                    unsigned short voxelImageColumns = 0;
                    unsigned short voxelImageRows = 0;
                    unsigned short voxelImageDepth = 0;
                    unsigned slice = 0;

                    bool isDefunct;
                    while (SUCCEEDED(fileQueue.get().IsDefunct(&isDefunct)) && !isDefunct)
                    {
                        std::shared_ptr<DicomFile> file;
                        FAIL_FAST_IF_FAILED(fileQueue.get().Dequeue(&file));

                        if (!spMeanBuffer)
                        {
                            FAIL_FAST_IF_FAILED(GetVoxelDimensions(file, nFiles,
                                voxelWidthInMillimeters, voxelHeightInMillimeters, voxelDepthInMillimeters,
                                &voxelImageColumns, &voxelImageRows, &voxelImageDepth));

                            Log(L"Creating resources for output buffer: (%d, %d, %d)", voxelImageColumns, voxelImageRows, voxelImageDepth);

                            unsigned numElements = voxelImageColumns * voxelImageRows * voxelImageDepth;
                            unsigned elementSize = sizeof(float);
                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(elementSize, numElements, nullptr, &spMeanBuffer));
                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(elementSize, numElements, nullptr, &spMeanSquaredBuffer));

                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferUAV(spMeanBuffer.Get(), &spMeanBufferUAV));
                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferUAV(spMeanSquaredBuffer.Get(), &spMeanSquaredBufferUAV));

                            // Counts
                            std::vector<unsigned> zeroOutBufferCount(numElements, 0);
                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(sizeof(unsigned), numElements, &zeroOutBufferCount[0], &spOutBufferCounts));
                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferUAV(spOutBufferCounts.Get(), &spOutBufferCountsUAV));

                            Log(L"Created resources for output buffer.");
                        }

                        Log(L"Processing: %ls", file->SafeGetFilename().c_str());

                        // Input buffer
                        // Get data as structured buffer
                        // Because structured buffers require a minumum size of 4 bytes per element,
                        // 2 pixels are packed together.
                        auto data = Property<ImageProperty::PixelData>::SafeGet(file);
                        Microsoft::WRL::ComPtr<ID3D11Buffer> spBuffer;
                        FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(
                            sizeof(short) * 2 /* size of item */,
                            static_cast<unsigned>(data->size() / 4) /* num items */,
                            &data->at(0) /* data */,
                            &spBuffer));

                        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView;
                        FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferSRV(spBuffer.Get(), &spShaderResourceView));

                        // Create means constant buffers
                        struct {
                            unsigned InputRCD[3];
                            unsigned OutputRCD[3];
                            float SpacingXYZ[3];
                            float VoxelSpacingXYZ[3];
                        } constantMeansData;
                        constantMeansData.InputRCD[0] = static_cast<unsigned>(Property<ImageProperty::Rows>::SafeGet(file));
                        constantMeansData.InputRCD[1] = static_cast<unsigned>(Property<ImageProperty::Columns>::SafeGet(file));
                        constantMeansData.InputRCD[2] = slice++;
                        constantMeansData.OutputRCD[1] = voxelImageColumns;
                        constantMeansData.OutputRCD[2] = voxelImageDepth;
                        constantMeansData.OutputRCD[0] = voxelImageRows;
                        constantMeansData.SpacingXYZ[0] = Property<ImageProperty::Spacings>::SafeGet(file)[0];
                        constantMeansData.SpacingXYZ[1] = Property<ImageProperty::Spacings>::SafeGet(file)[1];
                        constantMeansData.SpacingXYZ[2] = Property<ImageProperty::Spacings>::SafeGet(file)[2];
                        constantMeansData.VoxelSpacingXYZ[0] = static_cast<float>(voxelWidthInMillimeters);
                        constantMeansData.VoxelSpacingXYZ[1] = static_cast<float>(voxelHeightInMillimeters);
                        constantMeansData.VoxelSpacingXYZ[2] = static_cast<float>(voxelDepthInMillimeters);
                                
                        Microsoft::WRL::ComPtr<ID3D11Buffer> spMeanConstantBuffer;
                        FAIL_FAST_IF_FAILED(resources.get().CreateConstantBuffer(constantMeansData, &spMeanConstantBuffer));

                        std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs =
                            { spMeanBufferUAV, spOutBufferCountsUAV, spMeanSquaredBufferUAV };

                        std::vector<ID3D11ShaderResourceView*> sharedResourceViews =
                            { spShaderResourceView.Get() };

                        resources.get().RunComputeShader(spMeansComputeShader.Get(),
                            spMeanConstantBuffer.Get(), 1, &sharedResourceViews[0],
                            uavs, voxelImageColumns * voxelImageRows * voxelImageDepth, 1, 1);
                    }

                    // Square the means
                    Microsoft::WRL::ComPtr<ID3D11ComputeShader> spSquareComputeShader;
                    RETURN_IF_FAILED(resources.get().CreateComputeShader(
                        GetShaderFromPath(L"Shaders\\square_image.hlsl").c_str(),
                        "CSMain", &spSquareComputeShader));

                    std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> squareUAVs =
                        { spMeanBufferUAV };

                    resources.get().RunComputeShader(spSquareComputeShader.Get(), nullptr, 0, nullptr,
                        squareUAVs, voxelImageColumns * voxelImageRows * voxelImageDepth, 1, 1);

                    // subtract the means squared from the squared means
                    Microsoft::WRL::ComPtr<ID3D11ComputeShader> spAddComputeShader;
                    RETURN_IF_FAILED(resources.get().CreateComputeShader(
                        GetShaderFromPath(L"Shaders\\add_images.hlsl").c_str(),
                        "CSMain", &spAddComputeShader));

                    struct {
                        float Factor;
                        unsigned UNUSED[3];
                    } constantAddData = { -1.f };

                    Microsoft::WRL::ComPtr<ID3D11Buffer> spAddConstantBuffer;
                    FAIL_FAST_IF_FAILED(resources.get().CreateConstantBuffer(constantAddData, &spAddConstantBuffer));

                    Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
                    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUAV;

                    unsigned numElements = voxelImageColumns * voxelImageRows * voxelImageDepth;
                    unsigned elementSize = sizeof(float);
                    FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(elementSize, numElements, nullptr, &spOutBuffer));
                    FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferUAV(spOutBuffer.Get(), &spOutBufferUAV));

                    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spMeanBufferSRV;
                    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spMeanSquaredBufferSRV;
                    FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferSRV(spMeanBuffer.Get(), &spMeanBufferSRV));
                    FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferSRV(spMeanSquaredBuffer.Get(), &spMeanSquaredBufferSRV));

                    std::vector<ID3D11ShaderResourceView*> sharedResourceViews =
                        { spMeanSquaredBufferSRV.Get(), spMeanBufferSRV.Get() };

                    std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> outUAVs =
                        { spOutBufferUAV };

                    resources.get().RunComputeShader(spAddComputeShader.Get(), spAddConstantBuffer.Get(), 2, &sharedResourceViews[0],
                        outUAVs, voxelImageColumns * voxelImageRows * voxelImageDepth, 1, 1);

                    // Take sqrt of variance to get stddev
                    Microsoft::WRL::ComPtr<ID3D11ComputeShader> spSqrtComputeShader;
                    RETURN_IF_FAILED(resources.get().CreateComputeShader(
                        GetShaderFromPath(L"Shaders\\sqrt_image.hlsl").c_str(),
                        "CSMain", &spSqrtComputeShader));

                    resources.get().RunComputeShader(spSqrtComputeShader.Get(), nullptr, 0, nullptr,
                        outUAVs, voxelImageColumns * voxelImageRows * voxelImageDepth, 1, 1);

                    RETURN_IF_FAILED(
                        SaveToFile(
                            resources,
                            spOutBuffer.Get(),
                            voxelImageColumns * voxelImageDepth,
                            voxelImageRows,
                            sizeof(float),
                            outputFile.c_str()));
        
                        return S_OK;
                    }();
                },
                std::ref(resources), std::ref(fileQueue), nFiles,
                m_xInMillimeters, m_yInMillimeters, m_zInMillimeters,
                m_outputFile);

        t1.join();
        t2.join();
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::VoxelizeStdDev>() { Log(L"[OperationType::VoxelizeStdDev]"); }

} // Operations
} // DCM