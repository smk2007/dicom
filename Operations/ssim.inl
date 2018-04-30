#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::SSIM> 
{
    std::wstring m_outputFile;

    std::wstring m_xFolder;
    std::wstring m_yFolder;

    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    Operation(
        const std::wstring& xFolder,
        const std::wstring& yFolder,
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters,
        const std::wstring& outputFile) :
            m_xFolder(xFolder),
            m_yFolder(yFolder),
            m_xInMillimeters(xInMillimeters),
            m_yInMillimeters(yInMillimeters),
            m_zInMillimeters(zInMillimeters),
            m_outputFile(outputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {

        std::wstring xMeanFile = m_outputFile + L".xmean.dd";
        std::wstring yMeanFile = m_outputFile + L".ymean.dd";
        std::wstring xStdDevFile = m_outputFile + L".xstddev.dd";
        std::wstring yStdDevFile = m_outputFile + L".ystddev.dd";
        std::wstring xyCovarianceFile = m_outputFile + L".xycov.dd";

        MakeOperation<OperationType::VoxelizeStdDev>(m_xFolder, xStdDevFile, m_xInMillimeters, m_yInMillimeters, m_zInMillimeters, xMeanFile)->Run(resources);
        MakeOperation<OperationType::VoxelizeStdDev>(m_yFolder, yStdDevFile, m_xInMillimeters, m_yInMillimeters, m_zInMillimeters, yMeanFile)->Run(resources);
        MakeOperation<OperationType::VoxelizeCovariance>(m_xFolder, m_yFolder, xMeanFile, yMeanFile, m_xInMillimeters, m_yInMillimeters, m_zInMillimeters, xyCovarianceFile)->Run(resources);


        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::SSIM>() { Log(L"[OperationType::SSIM]"); }

} // Operations
} // DCM