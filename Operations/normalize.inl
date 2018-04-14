#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::Normalize> 
{
    Operation()
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::Normalize>() { Log(L"[OperationType::Normalize]"); }

} // Operations
} // DCM