// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "precomp.h"

using namespace Application::Infrastructure;
using namespace DCM::Operations;

static const struct
{
    const wchar_t* Name;
    unsigned NArgs;
    bool IsRequired;
} DCPArguments[] =
{

{ L"--gfactor-rvalue",         1, false },
{ L"--image-average",          0, false },
{ L"--image-convert-to-csv",   0, false },
{ L"--input-folder",           1, false },
{ L"--input-folder2",          1, false },
{ L"--input-file",             1, false },
{ L"--input-file2",            1, false },
{ L"--image-gfactor",          2, false },
{ L"--image-snr",              2, false },
{ L"--image-snr-factor",       1, false },
{ L"--voxelize-ssim",          3, false },
{ L"--gfactor-ssim",           3, false },
{ L"--gfactor-ssim-depth",     1, false },
{ L"--output-file",            1, false },
{ L"--output-file2",           1, false },
{ L"--normalize-image",        0, false },
{ L"--voxelize-mean",          3, false },
{ L"--voxelize-stddev",        3, false }

};

bool DirectoryExists(const std::wstring& path)
{
    DWORD dwAttrib = GetFileAttributes(path.c_str());

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool FileExists(const std::wstring& path)
{
    DWORD dwAttrib = GetFileAttributes(path.c_str());

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

class DCPApplication : public ApplicationBase<DCPApplication>
{
    HRESULT EnsureOption(const wchar_t* pOptionName)
    {
        bool isSet;
        RETURN_IF_FAILED(IsOptionSet(pOptionName, &isSet));
        if (!isSet)
        {
            RETURN_IF_FAILED(PrintInvalidArgument(true, pOptionName, L"Option not set."));
            return E_FAIL;
        }

        return S_OK;
    }
public:

    template <OperationType TType, typename... TArgs>
    HRESULT RunOperation(TArgs&&... args)
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        {
            Application::Infrastructure::DeviceResources resources;
            auto spOperation = MakeOperation<TType>(std::forward<TArgs>(args)...);

            LogOperation<TType>();

            spOperation->Run(resources);
        }
        CoUninitialize();
        return S_OK;
    }

    template <OperationType TType>
    HRESULT TryRunVoxelize(const std::wstring inputFolder, const std::wstring outputFile)
    {

        bool isSet;
        unsigned x, y, z;
        if (SUCCEEDED(IsOptionSet(pVoxelizeParameter, &isSet)) && isSet &&
            SUCCEEDED(GetOptionParameterAt<0>(pVoxelizeParameter, &x)) &&
            SUCCEEDED(GetOptionParameterAt<1>(pVoxelizeParameter, &y)) &&
            SUCCEEDED(GetOptionParameterAt<2>(pVoxelizeParameter, &z)))
        {
            RETURN_IF_FAILED(RunOperation<TType>(inputFolder, outputFile, x, y, z));
            return S_OK;
        }
        return S_FALSE;
    }

    HRESULT Run()
    {
        bool isSet;

        // All options output a file
        std::wstring outputFile;
        RETURN_IF_FAILED(EnsureOption(L"--output-file"));
        RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--output-file", &outputFile));

        // Check which options require file input
        std::wstring inputFile;
        if ((SUCCEEDED(IsOptionSet(L"--normalize-image", &isSet)) && isSet) || 
            (SUCCEEDED(IsOptionSet(L"--image-convert-to-csv", &isSet)) && isSet) ||
            (SUCCEEDED(IsOptionSet(L"--gfactor-ssim", &isSet)) && isSet))
        {
            RETURN_IF_FAILED(EnsureOption(L"--input-file"));            
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--input-file", &inputFile));
        }

        // Check which options require 2 file input
        std::wstring inputFile2;
        if ((SUCCEEDED(IsOptionSet(L"--gfactor-ssim", &isSet)) && isSet))
        {
            RETURN_IF_FAILED(EnsureOption(L"--input-file2"));
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--input-file2", &inputFile2));
        }

        // Check which options require folder input
        std::wstring inputFolder;
        std::wstring inputFolder2;
        if ((SUCCEEDED(IsOptionSet(L"--voxelize-mean", &isSet)) && isSet) || 
            (SUCCEEDED(IsOptionSet(L"--voxelize-stddev", &isSet)) && isSet) ||
            (SUCCEEDED(IsOptionSet(L"--image-average", &isSet)) && isSet))
        {
            RETURN_IF_FAILED(EnsureOption(L"--input-folder"));
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--input-folder", &inputFolder));
        }

        if (SUCCEEDED(IsOptionSet(L"--gfactor-ssim", &isSet)) && isSet)
        {
            unsigned x, y, z;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--gfactor-ssim", &x));
            RETURN_IF_FAILED(GetOptionParameterAt<1>(L"--gfactor-ssim", &y));
            RETURN_IF_FAILED(GetOptionParameterAt<2>(L"--gfactor-ssim", &z));
            unsigned depth;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--gfactor-ssim-depth", &depth));
            RETURN_IF_FAILED(RunOperation<OperationType::GFactorSSIM>(inputFile, inputFile2, x, y, z, depth, outputFile));
        }

        if (SUCCEEDED(IsOptionSet(L"--voxelize-ssim", &isSet)) && isSet)
        {
            unsigned x, y, z;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--voxelize-ssim", &x));
            RETURN_IF_FAILED(GetOptionParameterAt<1>(L"--voxelize-ssim", &y));
            RETURN_IF_FAILED(GetOptionParameterAt<2>(L"--voxelize-ssim", &z));
            RETURN_IF_FAILED(EnsureOption(L"--input-folder"));
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--input-folder", &inputFolder));
            RETURN_IF_FAILED(EnsureOption(L"--input-folder2"));
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--input-folder2", &inputFolder2));
            RETURN_IF_FAILED(RunOperation<OperationType::SSIM>(inputFolder, inputFolder2, x, y, z, outputFile));
            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--voxelize-mean", &isSet)) && isSet)
        {
            RETURN_IF_FAILED(IsOptionSet(L"--voxelize-mean", &isSet));
            RETURN_HR_IF_FALSE(E_FAIL, isSet);
            unsigned x, y, z;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--voxelize-mean", &x));
            RETURN_IF_FAILED(GetOptionParameterAt<1>(L"--voxelize-mean", &y));
            RETURN_IF_FAILED(GetOptionParameterAt<2>(L"--voxelize-mean", &z));
            RETURN_IF_FAILED(RunOperation<OperationType::VoxelizeMeans>(inputFolder, outputFile, x, y, z));
            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--voxelize-stddev", &isSet)) && isSet)
        {
            std::wstring outFile2;
            if (FAILED(GetOptionParameterAt<0>(L"--output-file2", &outFile2)))
            {
                outFile2.clear();
            }

            RETURN_IF_FAILED(IsOptionSet(L"--voxelize-stddev", &isSet));
            RETURN_HR_IF_FALSE(E_FAIL, isSet);
            unsigned x, y, z;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--voxelize-stddev", &x));
            RETURN_IF_FAILED(GetOptionParameterAt<1>(L"--voxelize-stddev", &y));
            RETURN_IF_FAILED(GetOptionParameterAt<2>(L"--voxelize-stddev", &z));
            RETURN_IF_FAILED(RunOperation<OperationType::VoxelizeStdDev>(inputFolder, outputFile, x, y, z, outFile2));
            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--normalize-image", &isSet)) && isSet)
        {
            float min, max;
            if (SUCCEEDED(GetOptionParameterAt<0>(L"--voxelize-stddev", &min)) &&
                SUCCEEDED(GetOptionParameterAt<1>(L"--voxelize-stddev", &max)))
            {
                RETURN_IF_FAILED(RunOperation<OperationType::Normalize>(inputFile, outputFile, min, max));
            }
            else
            {
                RETURN_IF_FAILED(RunOperation<OperationType::Normalize>(inputFile, outputFile));
            }
            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--image-average", &isSet)) && isSet)
        {

            RETURN_IF_FAILED(RunOperation<OperationType::AverageImages>(inputFolder, outputFile));
            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--image-convert-to-csv", &isSet)) && isSet)
        {
            RETURN_IF_FAILED(RunOperation<OperationType::ImageToCsv>(inputFile, outputFile));
            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--image-snr", &isSet)) && isSet)
        {
            std::wstring signalFile;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--image-snr", &signalFile));

            std::wstring noiseFile;
            RETURN_IF_FAILED(GetOptionParameterAt<1>(L"--image-snr", &noiseFile));

            float factor = 1.0f;
            if (SUCCEEDED(IsOptionSet(L"--image-snr-factor", &isSet)) && isSet)
            {
                RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--image-snr-factor", &factor));
            }
            RETURN_IF_FAILED(RunOperation<OperationType::SignalToNoise>(signalFile, noiseFile, outputFile, factor));
            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--image-gfactor", &isSet)) && isSet)
        {
            std::wstring techniqueSignalToNoiseFile;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--image-gfactor", &techniqueSignalToNoiseFile));

            std::wstring nonAcceleratedSignalToNoiseFile;
            RETURN_IF_FAILED(GetOptionParameterAt<1>(L"--image-gfactor", &nonAcceleratedSignalToNoiseFile));

            RETURN_IF_FAILED(EnsureOption(L"--gfactor-rvalue"));

            float rvalue;
            RETURN_IF_FAILED(GetOptionParameterAt<0>(L"--gfactor-rvalue", &rvalue));

            RETURN_IF_FAILED(
                RunOperation<OperationType::GFactor>(
                    techniqueSignalToNoiseFile, nonAcceleratedSignalToNoiseFile, rvalue, outputFile));

            return S_OK;
        }

        if (SUCCEEDED(IsOptionSet(L"--voxelize-ssim", &isSet)) && isSet)
        {
            return E_NOTIMPL;
        }

        return E_FAIL;
    }

    HRESULT GetLengthOfArgumentsToFollow(wchar_t* pwzArgName, unsigned* nArgsToFollow)
    {
        RETURN_HR_IF_NULL(E_POINTER, nArgsToFollow);
        RETURN_HR_IF_NULL(E_POINTER, pwzArgName);

        *nArgsToFollow = 0;

        auto foundIt = 
            std::find_if(
                std::begin(DCPArguments),
                std::end(DCPArguments),
                [pwzArgName](const auto& argument)
                {
                    return _wcsicmp(argument.Name, pwzArgName) == 0;
                }
            );

        RETURN_HR_IF(E_INVALIDARG, foundIt == std::end(DCPArguments));

        // Set the number of return args to follow
        *nArgsToFollow = foundIt->NArgs;

        return S_OK;
    };

    HRESULT ValidateArgument(const wchar_t* pArgumentName, bool* isValid)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pArgumentName);
        RETURN_HR_IF_NULL(E_POINTER, isValid);
        *isValid = true;

        if (_wcsicmp(pArgumentName, L"--input-folder") == 0 ||
            _wcsicmp(pArgumentName, L"--input-folder2") == 0)
        {
            std::wstring inputFolder;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(pArgumentName, &inputFolder));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = DirectoryExists(inputFolder);
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--input-file") == 0 ||
            _wcsicmp(pArgumentName, L"--input-file2") == 0)
        {
            std::wstring inputFile;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(pArgumentName, &inputFile));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = FileExists(inputFile);
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--voxelize-mean") == 0)
        {
            unsigned tmp;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--voxelize-mean", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<1>(L"--voxelize-mean", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<2>(L"--voxelize-mean", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--voxelize-stddev") == 0)
        {
            unsigned tmp;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--voxelize-stddev", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<1>(L"--voxelize-stddev", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<2>(L"--voxelize-stddev", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--image-snr-factor") == 0)
        {
            float tmp;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--image-snr-factor", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--image-snr") == 0)
        {
            std::wstring signalFile;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--image-snr", &signalFile));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = FileExists(signalFile);

            std::wstring noiseFile;
            *isValid = SUCCEEDED(GetOptionParameterAt<1>(L"--image-snr", &noiseFile));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = FileExists(noiseFile);
        }

        if (_wcsicmp(pArgumentName, L"--gfactor-ssim") == 0)
        {
            unsigned tmp;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--gfactor-ssim", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<1>(L"--gfactor-ssim", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<2>(L"--gfactor-ssim", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--gfactor-ssim-depth") == 0)
        {
            unsigned tmp;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--gfactor-ssim-depth", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--voxelize-ssim") == 0)
        {
            unsigned tmp;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--voxelize-ssim", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<1>(L"--voxelize-ssim", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = SUCCEEDED(GetOptionParameterAt<2>(L"--voxelize-ssim", &tmp));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        if (_wcsicmp(pArgumentName, L"--image-gfactor") == 0)
        {
            std::wstring techniqueSignalToNoiseFile;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--image-gfactor", &techniqueSignalToNoiseFile));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = FileExists(techniqueSignalToNoiseFile);

            std::wstring nonAcceleratedSignalToNoiseFile;
            *isValid = SUCCEEDED(GetOptionParameterAt<1>(L"--image-gfactor", &nonAcceleratedSignalToNoiseFile));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
            *isValid = FileExists(nonAcceleratedSignalToNoiseFile);
        }

        if (_wcsicmp(pArgumentName, L"--gfactor-rvalue") == 0)
        {
            float rvalue;
            *isValid = SUCCEEDED(GetOptionParameterAt<0>(L"--gfactor-rvalue", &rvalue));
            RETURN_HR_IF_FALSE(S_OK, *isValid);
        }

        return S_OK;
    }

    HRESULT PrintHelp()
    {
        static const auto HELP = LR"()";
        wprintf(HELP);
        return S_OK;
    }
};

#include "dicom_file.h"
#include "dicom_image_helper.h"
int main()
{
    auto hr = DCPApplication::Execute();

    if (FAILED(hr))
    {
        printf("ERROR: %d\n", hr);
    }

    return hr;
}

