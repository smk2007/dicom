#pragma once

#include <iomanip>

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

        std::vector<float> data;
        unsigned width;
        unsigned height;
        unsigned channels;

        RETURN_IF_FAILED(
            GetBufferFromGrayscaleImage(
                resources,
                m_inputFile.c_str(),
                &data,
                &width,
                &height,
                &channels));

        RETURN_HR_IF_FALSE(E_FAIL, channels == 1);

        std::ofstream stream(m_outputFile.c_str(), std::ios_base::out | std::ios_base::trunc);
        for (unsigned row = 0; row < height; row++)
        {
            for (unsigned column = 0; column < width; column++)
            {
                stream << std::setprecision(std::numeric_limits<long double>::digits10 + 1) << static_cast<double>(data[row * width + column]);
                if (column == width - 1)
                {
                    stream << "\n";
                }
                else
                {
                    stream << ", ";
                }
            }
        }
        stream.flush();

        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::ImageToCsv>() { Log(L"[OperationType::ImageToCsv]"); }

} // Operations
} // DCM