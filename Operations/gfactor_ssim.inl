#pragma once

#include <vector>

namespace DCM
{
namespace Operations
{

template <> struct Operation<OperationType::GFactorSSIM> 
{
    std::wstring m_outputFile;

    std::wstring m_xFile;
    std::wstring m_yFile;

    unsigned m_xROI;
    unsigned m_yROI;
    unsigned m_zROI;
    unsigned m_depth;

    Operation(
        const std::wstring& xFile,
        const std::wstring& yFile,
        unsigned xROI,
        unsigned yROI,
        unsigned zROI,
        unsigned depth,
        const std::wstring& outputFile) :
            m_xFile(xFile),
            m_yFile(yFile),
            m_xROI(xROI),
            m_yROI(yROI),
            m_zROI(zROI),
            m_depth(depth),
            m_outputFile(outputFile)
    {
        FAIL_FAST_IF_FALSE(m_xROI == 2);
        FAIL_FAST_IF_FALSE(m_yROI == 2);
        FAIL_FAST_IF_FALSE(m_zROI == 2);
    }

    unsigned GetIndexForImagePos(
        unsigned width, unsigned depth,
        unsigned x, unsigned y, unsigned z)
    {
        return (width * depth * y) + (z * width) + x;
    }

    double ComputeMeanForSSIM(
        float* data, unsigned width, unsigned depth,
        unsigned x, unsigned y, unsigned z)
    {
        double sum =
            data[GetIndexForImagePos(width, depth, x, y, z)] +
            data[GetIndexForImagePos(width, depth, x, y + 1, z)] +
            data[GetIndexForImagePos(width, depth, x + 1, y, z)] +
            data[GetIndexForImagePos(width, depth, x + 1, y + 1, z)] +
            data[GetIndexForImagePos(width, depth, x, y, z + 1)] +
            data[GetIndexForImagePos(width, depth, x, y + 1, z + 1)] +
            data[GetIndexForImagePos(width, depth, x + 1, y, z + 1)] +
            data[GetIndexForImagePos(width, depth, x + 1, y + 1, z + 1)];
        return sum / 8;
    }

    double ComputeVarianceForSSIM(
        float* data, double mean, unsigned width, unsigned depth,
        unsigned x, unsigned y, unsigned z)
    {
        double sum =
            pow(mean - data[GetIndexForImagePos(width, depth, x, y, z)], 2) +
            pow(mean - data[GetIndexForImagePos(width, depth, x, y + 1, z)], 2) +
            pow(mean - data[GetIndexForImagePos(width, depth, x + 1, y, z)], 2) +
            pow(mean - data[GetIndexForImagePos(width, depth, x + 1, y + 1, z)], 2) +
            pow(mean - data[GetIndexForImagePos(width, depth, x, y, z + 1)], 2) +
            pow(mean - data[GetIndexForImagePos(width, depth, x, y + 1, z + 1)], 2) +
            pow(mean - data[GetIndexForImagePos(width, depth, x + 1, y, z + 1)], 2) +
            pow(mean - data[GetIndexForImagePos(width, depth, x + 1, y + 1, z + 1)], 2);
        return sum / 8;
    }

    double ProcessVoxelCovariance(
        float* xData,
        float* yData,
        double ux,
        double uy,
        unsigned width, unsigned depth,
        unsigned xindex, unsigned yindex, unsigned zindex,
        unsigned xsize, unsigned ysize, unsigned zsize)
    {
        double covariance = 0;

        for (unsigned x1 = 0; x1 < xsize; x1++)
            for (unsigned y1 = 0; y1 < ysize; y1++)
                for (unsigned z1 = 0; z1 < zsize; z1++)
                {
                    covariance +=
                        (xData[GetIndexForImagePos(width, depth, xindex + x1, yindex + y1, zindex + z1)] - ux) *
                        (yData[GetIndexForImagePos(width, depth, xindex + x1, yindex + y1, zindex + z1)] - uy);
                }
        covariance /= ((xsize * ysize * zsize)-1);
        return covariance;
    }

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        std::vector<float> xData;
        unsigned xWidth, xHeight, xChannels;
        RETURN_IF_FAILED(GetBufferFromGrayscaleImage(
            resources, m_xFile.c_str(), &xData, &xWidth, &xHeight, &xChannels));
        Microsoft::WRL::ComPtr<IWICBitmapSource> spYFileSource;

        std::vector<float> yData;
        unsigned yWidth, yHeight, yChannels;
        RETURN_IF_FAILED(GetBufferFromGrayscaleImage(
            resources, m_yFile.c_str(), &yData, &yWidth, &yHeight, &yChannels));

        RETURN_HR_IF_FALSE(E_FAIL, xWidth == yWidth);
        RETURN_HR_IF_FALSE(E_FAIL, xHeight == yHeight);
        RETURN_HR_IF_FALSE(E_FAIL, xChannels == yChannels);

        unsigned width = xWidth / m_depth;
        unsigned height = yHeight;
        unsigned depth = m_depth;

        unsigned ssimWidth = width - m_xROI + 1;
        unsigned ssimHeight = height - m_yROI + 1;
        unsigned ssimDepth = m_depth - m_zROI + 1;

        unsigned outputWidth = ssimWidth * ssimDepth;
        unsigned outputHeight = ssimHeight;

        double k1 = .01;
        double k2 = .03;

        double L = 2;

        double c1 = k1 * k1*L*L;
        double c2 = k2 * k2*L*L;

        std::vector<float> ssimImage(outputHeight*outputWidth);
        for (unsigned z = 0; z < ssimDepth; z++)
        {
            for (unsigned x = 0; x < ssimWidth; x++)
            {
                for (unsigned y = 0; y < ssimHeight; y++)
                {
                    double ux = ComputeMeanForSSIM(xData.data(), width, depth, x, y, z);
                    double sx = sqrt(ComputeVarianceForSSIM(xData.data(), ux, width, depth, x, y, z));

                    double uy = ComputeMeanForSSIM(yData.data(), width, depth, x, y, z);
                    double sy = sqrt(ComputeVarianceForSSIM(yData.data(), uy, width, depth, x, y, z));

                    double xy_covariance = ProcessVoxelCovariance(
                        xData.data(),
                        yData.data(),
                        ux, uy,
                        width, depth,
                        x, y, z,
                        m_xROI, m_yROI, m_zROI);

                    ssimImage[GetIndexForImagePos(ssimWidth, ssimDepth, x, y, z)] =
                        ((2 * ux * uy) + c1) * (2 * xy_covariance + c2) /
                        (((ux * ux) + (uy * uy) + c1) * (sx*sx + sy*sy + c2));
                }
            }
        }

        RETURN_IF_FAILED(
            SaveToFile(
                ssimImage.data(),
                outputWidth,
                outputHeight,
                sizeof(float),
                m_outputFile.c_str()));
        return S_OK;
    }
};

template <> void inline LogOperation<OperationType::GFactorSSIM>() { Log(L"[OperationType::GFactorSSIM]"); }

} // Operations
} // DCM