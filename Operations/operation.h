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
    ConvertToFloat,
    VoxelizeMeans,
    VoxelizeStdDev,
    VoxelizeCovariance,
    Normalize,
    AverageImages,
    MultiplyImages,
    ImageToCsv,
    SignalToNoise,
    GFactor,
    SSIM,
    GFactorSSIM
};

template <unsigned TType> void LogOperation() {}
template <unsigned TType> struct Operation;

} // Operations
} // DCM

#include "errors.h"

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


#include "convert_to_float.inl"
#include "average_images.inl"
#include "multiply_images.inl"
#include "gfactor.inl"
#include "image_to_csv.inl"
#include "normalize.inl"
#include "signal_to_noise.inl"
#include "voxelize_means.inl"
#include "voxelize_stddev.inl"
#include "voxelize_covariance.inl"
#include "ssim.inl"
#include "gfactor_ssim.inl"

