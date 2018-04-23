#pragma once

#include "divide_images_helper.h"

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::SignalToNoise> 
{
    std::wstring m_signalFile;
    std::wstring m_noiseFile;
    std::wstring m_outputFile;

    Operation(
        const std::wstring& signalFile,
        const std::wstring& noiseFile,
        const std::wstring& outputFile) :
        m_signalFile(signalFile),
        m_noiseFile(noiseFile),
        m_outputFile(outputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        RETURN_IF_FAILED(DivideImages(resources, m_signalFile, m_noiseFile, m_outputFile));
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::SignalToNoise>() { Log(L"[OperationType::SignalToNoise]"); }

} // Operations
} // DCM