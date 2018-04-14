#pragma once

#include "voxelize_operation.h"

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::VoxelizeStdDev> : public VoxelizeOperation
{
    Operation(const std::wstring& inputFolder, unsigned xInMillimeters, unsigned yInMillimeters, unsigned zInMillimeters) :
        VoxelizeOperation(VoxelizeMode::StdDev, inputFolder, xInMillimeters, yInMillimeters, zInMillimeters)
    {}
};

template <> void inline LogOperation<OperationType::VoxelizeStdDev>() { Log(L"[OperationType::VoxelizeStdDev]"); }

} // Operations
} // DCM