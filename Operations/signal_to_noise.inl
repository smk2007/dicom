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
    float m_factor;

    Operation(
        const std::wstring& signalFile,
        const std::wstring& noiseFile,
        const std::wstring& outputFile,
        float factor = 1.0f) :
        m_signalFile(signalFile),
        m_noiseFile(noiseFile),
        m_outputFile(outputFile),
        m_factor(factor)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        printf("factor: %d", m_factor);

        RETURN_IF_FAILED(
            DivideImages(
                resources,
                m_signalFile,
                m_noiseFile,
                m_outputFile,
                static_cast<float>(m_factor / sqrt(2))));
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::SignalToNoise>() { Log(L"[OperationType::SignalToNoise]"); }

} // Operations
} // DCM