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
        Concurrency::ConcurrentQueue<std::shared_ptr<DCM::DicomFile>> dicomFiles(3);

        std::thread t1(
            [inputFolder = m_inputFolder]()
            {
                [&]()->HRESULT
                {
                    std::vector<std::wstring> children;
                    RETURN_IF_FAILED(GetChildren(inputFolder, &children));
                    return S_OK;
                }();
            });

        t1.join();

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