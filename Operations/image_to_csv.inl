#pragma once

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::ImageToCsv> 
{
    std::wstring m_inputFile;
    std::wstring m_outputFile;

    Operation(
        std::wstring inputFile,
        std::wstring outputFile) :
            m_inputFile(inputFile),
            m_outputFile(outputFile)
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