#pragma once

namespace DCM
{
namespace Operations
{

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

template <> void inline LogOperation<OperationType::SSIM>() { Log(L"[OperationType::SSIM]"); }

} // Operations
} // DCM