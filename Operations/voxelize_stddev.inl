#pragma once


namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::VoxelizeStdDev>
{
    // Voxel dimensions
    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    // Input/output variables
    std::wstring m_inputFolder;
    std::wstring m_outputFile;

    Operation(
        const std::wstring& inputFolder,
        const std::wstring& outputFile,
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters) :
        m_inputFolder(inputFolder),
        m_outputFile(outputFile),
        m_xInMillimeters(xInMillimeters),
        m_yInMillimeters(yInMillimeters),
        m_zInMillimeters(zInMillimeters)
    {}
 
    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        UNREFERENCED_PARAMETER(resources);
        wchar_t pwzFileName[MAX_PATH + 1];
        RETURN_HR_IF(E_FAIL, 0 == GetModuleFileName(NULL, pwzFileName, MAX_PATH + 1));
        std::wstring shaderPath(pwzFileName);
        shaderPath.erase(shaderPath.begin() + shaderPath.find_last_of(L'\\') + 1, shaderPath.end());
        shaderPath += L"Shaders\\voxelize_stddev.hlsl";
        //"CSMain",
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::VoxelizeStdDev>() { Log(L"[OperationType::VoxelizeStdDev]"); }

} // Operations
} // DCM