#pragma once

#include "divide_images_helper.h"

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::GFactor> 
{
    std::wstring m_techniqueSignalToNoiseFile;
    std::wstring m_nonAcceleratedSignalToNoiseFile;
    std::wstring m_outputFile;
    float m_factor;

    Operation(
        const std::wstring& techniqueSignalToNoiseFile,
        const std::wstring& nonAcceleratedSignalToNoiseFile,
        float factor,
        const std::wstring& outputFile) :
        m_techniqueSignalToNoiseFile(techniqueSignalToNoiseFile),
        m_nonAcceleratedSignalToNoiseFile(nonAcceleratedSignalToNoiseFile),
        m_factor(factor),
        m_outputFile(outputFile)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        RETURN_IF_FAILED(
            DivideImages(
                resources,
                m_nonAcceleratedSignalToNoiseFile,
                m_techniqueSignalToNoiseFile,
                m_outputFile,
                1 / sqrt(m_factor) /*factor*/));
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::GFactor>() { Log(L"[OperationType::GFactor]"); }

} // Operations
} // DCM