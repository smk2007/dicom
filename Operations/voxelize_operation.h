#pragma once

#include "voxel_types.h"

namespace DCM
{
namespace Operations
{

struct VoxelizeOperation
{
private:
    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    std::wstring m_inputFolder;

public:
    VoxelizeOperation(
        VoxelizeMode,
        const std::wstring& inputFolder,
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters) :
        m_inputFolder(inputFolder),
        m_xInMillimeters(xInMillimeters),
        m_yInMillimeters(yInMillimeters),
        m_zInMillimeters(zInMillimeters)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        // Create the shader
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
        RETURN_IF_FAILED(resources.CreateComputeShader(L"Shaders\\VoxelizeMeans.hlsl", "CSMain", &spComputeShader));

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
            auto voxelDepthInMillimeters)
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
                        L"outfile.bmp"));

                return S_OK;
            }();
        },
            std::ref(resources), spComputeShader, std::ref(fileQueue), nFiles,
            m_xInMillimeters, m_yInMillimeters, m_zInMillimeters);

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