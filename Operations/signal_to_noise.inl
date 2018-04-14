#pragma once

namespace DCM
{
namespace Operations
{

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

template <> void inline LogOperation<OperationType::SignalToNoise>() { Log(L"[OperationType::SignalToNoise]"); }

} // Operations
} // DCM