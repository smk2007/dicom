#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::VoxelizeCovariance>
{
    std::wstring m_outputFile;

    std::wstring m_xFolder;
    std::wstring m_yFolder;
    std::wstring m_xMeansFile;
    std::wstring m_yMeansFile;

    unsigned m_voxelWidthInMillimeters;
    unsigned m_voxelHeightInMillimeters;
    unsigned m_voxelDepthInMillimeters;

    Operation(
        const std::wstring& xFolder,
        const std::wstring& yFolder,
        const std::wstring& xMeansFile,
        const std::wstring& yMeansFile,
        unsigned voxelWidthInMillimeters,
        unsigned voxelHeightInMillimeters,
        unsigned voxelDepthInMillimeters,
        const std::wstring& outputFile) :
        m_xFolder(xFolder),
        m_yFolder(yFolder),
        m_xMeansFile(xMeansFile),
        m_yMeansFile(yMeansFile),
        m_voxelWidthInMillimeters(voxelWidthInMillimeters),
        m_voxelHeightInMillimeters(voxelHeightInMillimeters),
        m_voxelDepthInMillimeters(voxelDepthInMillimeters),
        m_outputFile(outputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        unsigned short voxelImageColumns = 0;
        unsigned short voxelImageRows = 0;
        unsigned short voxelImageDepth = 0;

        Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
        auto multiplyMeansOp = MakeOperation<OperationType::MultiplyImages>(
            m_xMeansFile, m_yMeansFile, L"", false);
        multiplyMeansOp->Run(resources);
        multiplyMeansOp->GetBuffer();

        std::vector<std::shared_ptr<DicomFile>> xMetadataFiles;
        RETURN_IF_FAILED(GetMetadataFiles(m_xFolder, &xMetadataFiles));
        RETURN_IF_FAILED(SortFilesInScene(&xMetadataFiles));
        auto nFiles = static_cast<unsigned>(xMetadataFiles.size());

        std::vector<std::shared_ptr<DicomFile>> yMetadataFiles;
        RETURN_IF_FAILED(GetMetadataFiles(m_yFolder, &yMetadataFiles));
        RETURN_IF_FAILED(SortFilesInScene(&yMetadataFiles));
        RETURN_HR_IF_FAILED(E_FAIL, nFiles == static_cast<unsigned>(yMetadataFiles.size()));

        using DicomPair = std::pair<std::shared_ptr<DicomFile>, std::shared_ptr<DicomFile>>;
        Concurrency::ConcurrentQueue<DicomPair> fileQueue(100);

        std::thread t1([&]() {
        [&]() -> HRESULT
        {
            for (unsigned i = 0; i < xMetadataFiles.size(); i++)
            {
                auto xFile = xMetadataFiles[i];
                auto yFile = yMetadataFiles[i];
                // Get the full file
                std::shared_ptr<DicomFile> xFullFile, yFullFile;
                FAIL_FAST_IF_FAILED(MakeDicomImageFile(xFile->SafeGetFilename(), &xFullFile));
                FAIL_FAST_IF_FAILED(MakeDicomImageFile(yFile->SafeGetFilename(), &yFullFile));
                FAIL_FAST_IF_FAILED(fileQueue.Enqueue(std::make_pair(std::move(xFullFile), std::move(yFullFile))));
            }

            fileQueue.Finish();
            return S_OK;
        }();
        });

        std::thread t2([&]() {
        [&]() -> HRESULT
        {
            Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBufferCounts;
            std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs;
            unsigned slice = 0;

            bool isDefunct;
            while (SUCCEEDED(fileQueue.IsDefunct(&isDefunct)) && !isDefunct)
            {
                DicomPair pair;
                FAIL_FAST_IF_FAILED(fileQueue.Dequeue(&pair));

                if (!spOutBuffer || uavs.size() == 0)
                {
                    FAIL_FAST_IF_FAILED(GetVoxelDimensions(
                        pair.first, nFiles,
                        m_voxelWidthInMillimeters, m_voxelHeightInMillimeters, m_voxelDepthInMillimeters,
                        &voxelImageColumns, &voxelImageRows, &voxelImageDepth));

                    Log(L"Creating resources for output buffer: (%d, %d, %d)", voxelImageColumns, voxelImageRows, voxelImageDepth);

                    // Mean buffer
                    FAIL_FAST_IF_FAILED(resources.CreateStructuredBuffer(
                        sizeof(float) /* size of item */,
                        voxelImageColumns * voxelImageRows * voxelImageDepth /* num items */,
                        nullptr/* data */,
                        &spOutBuffer));

                    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
                    FAIL_FAST_IF_FAILED(resources.CreateStructuredBufferUAV(
                            spOutBuffer.Get(), &spOutBufferUnorderedAccessView));

                    // Counts
                    std::vector<unsigned> zeroOutBufferCount(voxelImageColumns * voxelImageRows * voxelImageDepth, 0);
                    FAIL_FAST_IF_FAILED(
                        resources.CreateStructuredBuffer(
                            sizeof(unsigned) /* size of item */,
                            voxelImageColumns * voxelImageRows * voxelImageDepth /* num items */,
                            &zeroOutBufferCount[0],
                            &spOutBufferCounts));

                    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferCountsUnorderedAccessView;
                    FAIL_FAST_IF_FAILED(resources.CreateStructuredBufferUAV(
                        spOutBufferCounts.Get(), &spOutBufferCountsUnorderedAccessView));

                    uavs = { spOutBufferUnorderedAccessView, spOutBufferCountsUnorderedAccessView };

                    Log(L"Created resources for output buffer.");
                }

                Log(L"Processing: %ls and %ls", pair.first->SafeGetFilename().c_str(), pair.second->SafeGetFilename().c_str());

                auto multiplyOp = MakeOperation<OperationType::MultiplyImages>(
                    pair.first->SafeGetFilename(), pair.second->SafeGetFilename(),
                    L"", false);
                multiplyOp->Run(resources);
                auto pBuffer = multiplyOp->GetBuffer();

                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView;
                FAIL_FAST_IF_FAILED(resources.CreateStructuredBufferSRV(
                    pBuffer, &spShaderResourceView));

                std::vector<ID3D11ShaderResourceView*> sharedResourceViews = { spShaderResourceView.Get() };

                Log(L"Creating constant resources.");

                struct {
                    unsigned InputRCD[3];
                    unsigned OutputRCD[3];
                    float SpacingXYZ[3];
                    float VoxelSpacingXYZ[3];
                } constantData =
                {
                    {
                        static_cast<unsigned>(Property<ImageProperty::Rows>::SafeGet(pair.first)),
                        static_cast<unsigned>(Property<ImageProperty::Columns>::SafeGet(pair.first)),
                        slice++
                    },
                    {
                        voxelImageRows,
                        voxelImageColumns,
                        voxelImageDepth
                    },
                    {
                        Property<ImageProperty::Spacings>::SafeGet(pair.first)[0],
                        Property<ImageProperty::Spacings>::SafeGet(pair.first)[1],
                        Property<ImageProperty::Spacings>::SafeGet(pair.first)[2]
                    },
                    {
                        static_cast<float>(m_voxelWidthInMillimeters),
                        static_cast<float>(m_voxelHeightInMillimeters),
                        static_cast<float>(m_voxelDepthInMillimeters)
                    }
                };

                Microsoft::WRL::ComPtr<ID3D11Buffer> spConstantBuffer;
                FAIL_FAST_IF_FAILED(resources.CreateConstantBuffer(constantData, &spConstantBuffer));

                Log(L"Created constant resources.");
                Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
                RETURN_IF_FAILED(CreateShader(resources, L"Shaders\\voxelize_mean_f.hlsl", &spComputeShader));
                resources.RunComputeShader(spComputeShader.Get(), spConstantBuffer.Get(), 1, &sharedResourceViews[0],
                    uavs, voxelImageColumns * voxelImageRows * voxelImageDepth, 1, 1);
            }

            return S_OK;
        }();
        });

        t1.join();
        t2.join();

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView1;
        FAIL_FAST_IF_FAILED(resources.CreateStructuredBufferSRV(
            spOutBuffer.Get(), &spShaderResourceView1));
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView2;
        FAIL_FAST_IF_FAILED(resources.CreateStructuredBufferSRV(
            multiplyMeansOp->GetBuffer(), &spShaderResourceView2));
        std::vector<ID3D11ShaderResourceView*> sharedResourceViews =
        { spShaderResourceView2.Get(), spShaderResourceView1.Get() };

        struct {
            float Factor;
            unsigned UNUSED[3];
        } constantAddData = { -1.f };

        Microsoft::WRL::ComPtr<ID3D11Buffer> spAddConstantBuffer;
        FAIL_FAST_IF_FAILED(resources.CreateConstantBuffer(
            constantAddData, &spAddConstantBuffer));

        Microsoft::WRL::ComPtr<ID3D11Buffer> spCovarianceBuffer;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spCovarianceBufferUAV;
        FAIL_FAST_IF_FAILED(resources.CreateStructuredBufferUAV(spCovarianceBuffer.Get(), &spCovarianceBufferUAV));

        std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> outUAVs =
        { spCovarianceBufferUAV };

        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
        RETURN_IF_FAILED(CreateShader(resources, L"Shaders\\add_images.hlsl", &spComputeShader));
        resources.RunComputeShader(spComputeShader.Get(), spAddConstantBuffer.Get(), 2, &sharedResourceViews[0],
            outUAVs, voxelImageColumns * voxelImageRows * voxelImageDepth, 1, 1);

        RETURN_IF_FAILED(
            SaveToFile(
                resources,
                spOutBuffer.Get(),
                voxelImageColumns * voxelImageDepth,
                voxelImageRows,
                sizeof(float),
                m_outputFile.c_str()));

        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::VoxelizeCovariance>() { Log(L"[OperationType::VoxelCovariance]"); }

} // Operations
} // DCM