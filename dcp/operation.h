/*
*
*   application.h
*
*   CRTP application header
*
*/

#pragma once

#include "errors.h"

namespace DCM
{
namespace Operations
{

enum OperationType
{
    VoxelizeMeans,
    VoxelizeStdDev,
    AverageImages,
    ConcatenateImages,
    ImageToCsv,
    SignalToNoise,
    GFactor,
    SSIM
};

template <unsigned TType> struct Operation;
template <> struct Operation<OperationType::AverageImages>
{
    std::wstring m_inputFolder;
    Operation(const std::wstring& inputFolder) :
        m_inputFolder(inputFolder)
    {}

    HRESULT Run()
    {
        return S_OK;
    }
};
template <> struct Operation<OperationType::ConcatenateImages>
{
    std::wstring m_inputFolder;
    Operation(const std::wstring& inputFolder) :
        m_inputFolder(inputFolder)
    {}

    HRESULT Run()
    {
        return S_OK;
    }
};
template <> struct Operation<OperationType::GFactor>
{
    Operation()
    {}

    HRESULT Run()
    {
        return S_OK;
    }
};
template <> struct Operation<OperationType::ImageToCsv>
{
    std::wstring m_inputFile;
    Operation(const std::wstring& inputFile) :
        m_inputFile(inputFile)
    {}

    HRESULT Run()
    {
        return S_OK;
    }
};
template <> struct Operation<OperationType::SignalToNoise>
{
    Operation()
    {}

    HRESULT Run()
    {
        return S_OK;
    }
};
template <> struct Operation<OperationType::SSIM>
{
    Operation()
    {}

    HRESULT Run()
    {
        return S_OK;
    }
};
template <> struct Operation<OperationType::VoxelizeMeans>
{
    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    std::wstring m_inputFolder;

    Operation(
        const std::wstring& inputFolder,
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters) :
            m_inputFolder(inputFolder),
            m_xInMillimeters(xInMillimeters),
            m_yInMillimeters(yInMillimeters),
            m_zInMillimeters(zInMillimeters)
    {}

    HRESULT Run()
    {
        Concurrency::ConcurrentQueue<std::shared_ptr<DCM::DicomFile>> dicomFiles(100);

        std::thread t1(
            [](auto inputFolder, auto files)
            {
                [&]()->HRESULT
                {
                    std::vector<std::wstring> children;
                    RETURN_IF_FAILED(GetChildren(inputFolder, &children));

                    unsigned i = 0;
                    for (auto file : children)
                    {
                        wprintf(L"Queuing %d: %ls \n", i++, file.c_str());
                     
                        std::shared_ptr<DCM::DicomFile> dicomFile;
                        RETURN_IF_FAILED(DCM::MakeDicomMetadataFile(file, &dicomFile));
                        RETURN_IF_FAILED(files.get().Enqueue(std::move(dicomFile)));
                    }

                    files.get().Finish();

                    return S_OK;
                }();
            }, m_inputFolder, std::ref(dicomFiles));

        std::thread t2(
            [](auto dicomFiles)
            {
                [&]()->HRESULT
                {
                    bool isDefunct;
                    unsigned i = 0;
                    while (SUCCEEDED(dicomFiles.get().IsDefunct(&isDefunct)) && !isDefunct)
                    {
                        std::shared_ptr<DCM::DicomFile> dicomFile;
                        RETURN_IF_FAILED(dicomFiles.get().Dequeue(&dicomFile));
                        const wchar_t* pName;
                        dicomFile->GetFilename(&pName);
                        wprintf(L"Processing %d: %ls \n", i++, pName);
                    }

                    return S_OK;
                }();
            }, std::ref(dicomFiles));



        t1.join();
        t2.join();

        //dicomFiles.Enqueue(DCM::DicomFile())
        return S_OK;
    }
};
template <> struct Operation<OperationType::VoxelizeStdDev>
{
    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    std::wstring m_inputFile;

    Operation(
        const std::wstring& inputFile, 
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters) :
            m_inputFile(inputFile),
            m_xInMillimeters(xInMillimeters),
            m_yInMillimeters(yInMillimeters),
            m_zInMillimeters(zInMillimeters)
    {}

    HRESULT Run()
    {
        return S_OK;
    }
};

template <unsigned TType, typename... TArgs>
std::shared_ptr<Operation<TType>> MakeOperation(TArgs&&... args)
{
    std::shared_ptr<Operation<TType>> spOperation(new Operation<TType>(std::forward<TArgs>(args)...));
    return spOperation;
}

}
}