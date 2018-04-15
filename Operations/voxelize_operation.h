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

        std::thread t2([](
            auto resources,
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
                Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutUnorderedAccessView;
                unsigned short voxelImageWidth = 0;
                unsigned short voxelImageHeight = 0;
                unsigned short voxelImageDepth = 0;
                unsigned slice = 0;

                bool isDefunct;
                while (SUCCEEDED(fileQueue.get().IsDefunct(&isDefunct)) && !isDefunct)
                {
                    std::shared_ptr<DicomFile> file;
                    FAIL_FAST_IF_FAILED(fileQueue.get().Dequeue(&file));

                    if (!spOutBuffer || !spOutUnorderedAccessView)
                    {
                        FAIL_FAST_IF_FAILED(GetVoxelDimensions(file, nFiles,
                            voxelWidthInMillimeters, voxelHeightInMillimeters, voxelDepthInMillimeters,
                            &voxelImageWidth, &voxelImageHeight, &voxelImageDepth));

                        Log(L"Creating resources for output buffer: (%d, %d, %d)", voxelImageWidth, voxelImageHeight, voxelImageDepth);
                        FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBuffer(
                            sizeof(float) /* size of item */,
                            voxelImageWidth * voxelImageHeight * voxelImageDepth /* num items */,
                            nullptr/* data */,
                            &spOutBuffer));

                        FAIL_FAST_IF_FAILED(resources.get().CreateStructuredBufferUAV(spOutBuffer.Get(), &spOutUnorderedAccessView));
                        Log(L"Created resources for output buffer.");
                    }

                    Log(L"Processing: %ls", file->SafeGetFilename().c_str());
                    Log(L"Creating constant resources.");

                    struct {
                        float ZPosition;
                        unsigned short Width;
                        unsigned short Height;
                        unsigned short Depth;
                        float Unused;
                    } constantData =
                    {
                        Property<ImageProperty::Spacings>::SafeGet(file)[2] * slice++,
                        voxelImageWidth,
                        voxelImageHeight,
                        voxelImageDepth
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
                        spOutUnorderedAccessView.Get(), voxelImageWidth * voxelImageHeight * voxelImageDepth, 1, 1);
                }

                WICPixelFormatGUID format = GUID_WICPixelFormat32bppGrayFloat;
                RETURN_IF_FAILED(
                    SaveToFile(
                        resources,
                        spOutBuffer.Get(),
                        voxelImageWidth * voxelImageDepth,
                        voxelImageHeight,
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
        unsigned short* voxelImageWidth,
        unsigned short* voxelImageHeight,
        unsigned short* voxelImageDepth)
    {
        RETURN_HR_IF_NULL(E_POINTER, voxelImageWidth);
        RETURN_HR_IF_NULL(E_POINTER, voxelImageHeight);
        RETURN_HR_IF_NULL(E_POINTER, voxelImageDepth);

        // Initialize the out buffer on the first frame
        auto spacings = Property<ImageProperty::Spacings>::SafeGet(spFile);
        auto columns = Property<ImageProperty::Columns>::SafeGet(spFile);
        auto rows = Property<ImageProperty::Rows>::SafeGet(spFile);
        auto rawVoxelImageWidth = static_cast<unsigned short>(ceil(spacings[0] * columns / voxelWidthInMillimeters));

        // Correct the width to be a multiple of 2
        *voxelImageWidth = (static_cast<unsigned>(rawVoxelImageWidth) % 2 == 0) ? rawVoxelImageWidth : rawVoxelImageWidth + 1;
        *voxelImageHeight = static_cast<unsigned short>(ceil(spacings[1] * rows / voxelHeightInMillimeters));
        *voxelImageDepth = static_cast<unsigned short>(ceil(spacings[2] * nFiles / voxelDepthInMillimeters));

        return S_OK;
    }
};

} // Operations
} // DCM