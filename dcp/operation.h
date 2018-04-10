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

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};
template <> struct Operation<OperationType::ConcatenateImages>
{
    std::wstring m_inputFolder;
    Operation(const std::wstring& inputFolder) :
        m_inputFolder(inputFolder)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};
template <> struct Operation<OperationType::GFactor>
{
    Operation()
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};
template <> struct Operation<OperationType::ImageToCsv>
{
    std::wstring m_inputFile;
    Operation(const std::wstring& inputFile) :
        m_inputFile(inputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};
template <> struct Operation<OperationType::SignalToNoise>
{
    Operation()
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};
template <> struct Operation<OperationType::SSIM>
{
    Operation()
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};

#include "voxelize_means.inl"

template <> struct Operation<OperationType::VoxelizeMeans> : public VoxelizeOperation<OperationType::VoxelizeMeans>
{
    Operation(const std::wstring& inputFolder, unsigned xInMillimeters, unsigned yInMillimeters, unsigned zInMillimeters) :
        VoxelizeOperation<OperationType::VoxelizeMeans>(inputFolder, xInMillimeters, yInMillimeters, zInMillimeters)
    {}
};

template <> struct Operation<OperationType::VoxelizeStdDev> : public VoxelizeOperation<OperationType::VoxelizeStdDev>
{
    Operation(const std::wstring& inputFolder, unsigned xInMillimeters, unsigned yInMillimeters, unsigned zInMillimeters) :
        VoxelizeOperation<OperationType::VoxelizeStdDev>(inputFolder, xInMillimeters, yInMillimeters, zInMillimeters)
    {}
};

template <unsigned TType, typename... TArgs>
std::shared_ptr<Operation<TType>> MakeOperation(TArgs&&... args)
{
    std::shared_ptr<Operation<TType>> spOperation(new Operation<TType>(std::forward<TArgs>(args)...));
    return spOperation;
}

}
}