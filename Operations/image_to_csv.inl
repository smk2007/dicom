#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::ImageToCsv> 
{
    Operation()
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::ImageToCsv>() { Log(L"[OperationType::ImageToCsv]"); }

} // Operations
} // DCM