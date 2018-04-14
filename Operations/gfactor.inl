#pragma once

namespace DCM
{
namespace Operations
{

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

template <> void inline LogOperation<OperationType::GFactor>() { Log(L"[OperationType::GFactor]"); }

} // Operations
} // DCM