/*
*
*   application.h
*
*   CRTP application header
*
*/

#pragma once

namespace DCM
{
namespace Operations
{

enum OperationType
{
    VoxelizeMeans,
    VoxelizeStdDev,
    Normalize,
    AverageImages,
    ConcatenateImages,
    ImageToCsv,
    SignalToNoise,
    GFactor,
    SSIM
};

template <unsigned TType> void LogOperation() {}
template <unsigned TType> struct Operation;

} // Operations
} // DCM

#include "errors.h"

#include "average_images.inl"
#include "concatenate_images.inl"
#include "gfactor.inl"
#include "image_to_csv.inl"
#include "normalize.inl"
#include "signal_to_noise.inl"
#include "ssim.inl"
#include "voxelize_means.inl"
#include "voxelize_stddev.inl"


namespace DCM
{
namespace Operations
{

template <unsigned TType, typename... TArgs>
std::shared_ptr<Operation<TType>> MakeOperation(TArgs&&... args)
{
    std::shared_ptr<Operation<TType>> spOperation(new Operation<TType>(std::forward<TArgs>(args)...));
    return spOperation;
}

} // Operations
} // DCM
