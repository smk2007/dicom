#pragma once

namespace Application
{
namespace Infrastructure
{

class DeviceResources
{
    Microsoft::WRL::ComPtr<ID3D11Device3> m_d3dDevice;
    Microsoft::WRL::ComPtr<IWICImagingFactory2>	m_wicFactory;

public:

    DeviceResources()
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&m_wicFactory)
        );
    }

    ~DeviceResources()
    {
        CoUninitialize();
    }

    ID3D11Device3*               GetD2DDevice()         const { return m_d3dDevice.Get(); }
    IWICImagingFactory2*         GetWicImagingFactory() const { return m_wicFactory.Get(); }
};

} // Infrastructure
} // Application