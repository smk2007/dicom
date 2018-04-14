#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::ConcatenateImages> 
{
    Operation()
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::ConcatenateImages>() { Log(L"[OperationType::ConcatenateImages]"); }

} // Operations
} // DCM