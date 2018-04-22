#pragma once

namespace DCM
{
namespace Operations
{

struct VoxelizeOperation
{
private:
    // Voxel dimensions
    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    // Input/output variables
    std::wstring m_inputFolder;
    std::wstring m_outputFile;

    // Shader variables
    std::wstring m_shaderPath;
    std::string m_shaderMain;

public:
    VoxelizeOperation(
        const wchar_t* pShaderPath,
        const char* pShaderMain,
        const std::wstring& inputFolder,
        const std::wstring& outputFile,
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters) :
        m_shaderPath(pShaderPath),
        m_shaderMain(pShaderMain),
        m_inputFolder(inputFolder),
        m_outputFile(outputFile),
        m_xInMillimeters(xInMillimeters),
        m_yInMillimeters(yInMillimeters),
        m_zInMillimeters(zInMillimeters)
    {
    }

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        wchar_t pwzFileName[MAX_PATH + 1];
        RETURN_HR_IF(E_FAIL, 0 == GetModuleFileName(NULL, pwzFileName, MAX_PATH + 1));
        std::wstring shaderPath(pwzFileName);
        shaderPath.erase(shaderPath.begin() + shaderPath.find_last_of(L'\\') + 1, shaderPath.end());
        shaderPath += m_shaderPath;

        // Create the shader
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
        RETURN_IF_FAILED(resources.CreateComputeShader(shaderPath.c_str(), m_shaderMain.c_str(), &spComputeShader));

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
               auto spComputeShader,
               auto fileQueue,
               auto nFiles,
               auto voxelWidthInMillimeters,
               auto voxelHeightInMillimeters,
               auto voxelDepthInMillimeters,
               auto outputFile)
            {
                [&]()->HRESULT
                {
                    Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBuffer;
                    Microsoft::WRL::ComPtr<ID3D11Buffer> spOutBufferCounts;
                    std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs;
                    unsigned short voxelImageColumns = 0;
                    unsigned short voxelImageRows = 0;
                    unsigned short voxelImageDepth = 0;
                    unsigned slice = 0;

                    bool isDefunct;
                    while (SUCCEEDED(fileQueue.get().IsDefunct(&isDefunct)) && !isDefunct)
                    {
                        std::shared_ptr<DicomFile> file;
                        FAIL_FAST_IF_FAILED(fileQueue.get().Dequeue(&file));

                        if (!spOutBuffer || uavs.size() == 0)
                        {
                            FAIL_FAST_IF_FAILED(GetVoxelDimensions(file, nFiles,
                                voxelWidthInMillimeters, voxelHeightInMillimeters, voxelDepthInMillimeters,
                                &voxelImageColumns, &voxelImageRows, &voxelImageDepth));

                            Log(L"Creating resources for output buffer: (%d, %d, %d)", voxelImageColumns, voxelImageRows, voxelImageDepth);
                            //std::vector<float> zeroOutBuffer(voxelImageColumns * voxelImageRows * voxelImageDepth, 0.f);
                            FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(
                                sizeof(float) /* size of item */,
                                voxelImageColumns * voxelImageRows * voxelImageDepth /* num items */,
                                nullptr,//&zeroOutBuffer[0]/* data */,
                                &spOutBuffer));

                            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
                            FAIL_FAST_IF_FAILED(
                                resources.get().CreateStructuredBufferUAV(
                                    spOutBuffer.Get(),
                                    &spOutBufferUnorderedAccessView));

                            //std::vector<unsigned> zeroOutBufferCount(voxelImageColumns * voxelImageRows * voxelImageDepth, 0);
                            FAIL_FAST_IF_FAILED(
                                resources.get().CreateStructuredBuffer(
                                    sizeof(unsigned) /* size of item */,
                                    voxelImageColumns * voxelImageRows * voxelImageDepth /* num items */,
                                    nullptr,//&zeroOutBufferCount[0],
                                    &spOutBufferCounts));

                            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferCountsUnorderedAccessView;
                            FAIL_FAST_IF_FAILED(
                                resources.get().CreateStructuredBufferUAV(
                                    spOutBufferCounts.Get(),
                                    &spOutBufferCountsUnorderedAccessView));

                            uavs = { spOutBufferUnorderedAccessView, spOutBufferCountsUnorderedAccessView };

                            Log(L"Created resources for output buffer.");
                        }

                        Log(L"Processing: %ls", file->SafeGetFilename().c_str());
                        Log(L"Creating constant resources.");

                        struct {
                            unsigned InputRCD[3];
                            unsigned OutputRCD[3];
                            float SpacingXYZ[3];
                            float VoxelSpacingXYZ[3];
                            unsigned Mode;
                            unsigned UNUSED[3];
                        } constantData =
                        {
                            {
                                static_cast<unsigned>(Property<ImageProperty::Rows>::SafeGet(file)),
                                static_cast<unsigned>(Property<ImageProperty::Columns>::SafeGet(file)),
                                slice++
                            },
                            {
                                voxelImageRows,
                                voxelImageColumns,
                                voxelImageDepth
                            },
                            {
                                Property<ImageProperty::Spacings>::SafeGet(file)[0],
                                Property<ImageProperty::Spacings>::SafeGet(file)[1],
                                Property<ImageProperty::Spacings>::SafeGet(file)[2]
                            },
                            {
                                static_cast<float>(voxelWidthInMillimeters),
                                static_cast<float>(voxelHeightInMillimeters),
                                static_cast<float>(voxelDepthInMillimeters)
                            },
                            0 // Normal addition
                        };

                        Microsoft::WRL::ComPtr<ID3D11Buffer> spConstantBuffer;
                        FAIL_FAST_IF_FAILED(resources.get().CreateConstantBuffer(constantData, &spConstantBuffer));

                        Log(L"Created constant resources.");

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

                        std::vector<ID3D11ShaderResourceView*> sharedResourceViews = { spShaderResourceView.Get() };
                        resources.get().RunComputeShader(spComputeShader.Get(), spConstantBuffer.Get(), 1, &sharedResourceViews[0],
                            uavs, voxelImageColumns * voxelImageRows * voxelImageDepth, 1, 1);
                    }

                    WICPixelFormatGUID format = GUID_WICPixelFormat32bppGrayFloat;
                    RETURN_IF_FAILED(
                        SaveToFile(
                            resources,
                            spOutBuffer.Get(),
                            voxelImageColumns * voxelImageDepth,
                            voxelImageRows,
                            sizeof(float),
                            format,
                            outputFile.c_str()));

                    return S_OK;
                }();
            },
            std::ref(resources), spComputeShader, std::ref(fileQueue), nFiles,
            m_xInMillimeters, m_yInMillimeters, m_zInMillimeters,
            m_outputFile);

        t1.join();
        t2.join();

        return S_OK;
    }

    static HRESULT GetVoxelDimensions(
        std::shared_ptr<DicomFile> spFile,
        unsigned nFiles,
        double voxelWidthInMillimeters,
        double voxelHeightInMillimeters,
        double voxelDepthInMillimeters,
        unsigned short* voxelImageColumns,
        unsigned short* voxelImageRows,
        unsigned short* voxelImageDepth)
    {
        RETURN_HR_IF_NULL(E_POINTER, voxelImageColumns);
        RETURN_HR_IF_NULL(E_POINTER, voxelImageRows);
        RETURN_HR_IF_NULL(E_POINTER, voxelImageDepth);

        // Initialize the out buffer on the first frame
        auto spacings = Property<ImageProperty::Spacings>::SafeGet(spFile);
        auto columns = Property<ImageProperty::Columns>::SafeGet(spFile);
        auto rows = Property<ImageProperty::Rows>::SafeGet(spFile);
        auto rawVoxelImageWidth = static_cast<unsigned short>(ceil(spacings[0] * columns / voxelWidthInMillimeters));

        // Correct the width to be a multiple of 2
        *voxelImageColumns = (static_cast<unsigned>(rawVoxelImageWidth) % 2 == 0) ? rawVoxelImageWidth : rawVoxelImageWidth + 1;
        *voxelImageRows = static_cast<unsigned short>(ceil(spacings[1] * rows / voxelHeightInMillimeters));
        *voxelImageDepth = static_cast<unsigned short>(ceil(spacings[2] * nFiles / voxelDepthInMillimeters));

        return S_OK;
    }
};

} // Operations
} // DCM