#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::AverageImages> 
{
    Operation()
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::AverageImages>() { Log(L"[OperationType::AverageImages]"); }

} // Operations
} // DCM