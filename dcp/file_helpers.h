#pragma once
//
// DicomFile.h
// Declaration of the App class.
//

#pragma once

namespace DCM
{

enum FileType { Directory, File };

inline HRESULT GetChildren(
    const std::wstring& input,
    std::vector<std::wstring>* children,
    FileType type = FileType::File)
{
    RETURN_HR_IF_NULL(E_POINTER, children);
    children->clear();

    namespace fs = std::experimental::filesystem;

    for (auto& file : fs::directory_iterator(input))
    {
        std::wstring path = file.path();
        DWORD dwAttrib = GetFileAttributes(path.c_str());

        if (type == FileType::Directory &&
            (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
        {
            children->push_back(std::move(path));
        }
        else if (type == FileType::File &&
                (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            children->push_back(std::move(path));
        }
    }

    return S_OK;
}

inline HRESULT GetMetadataFiles(std::wstring inputFolder, std::vector<std::shared_ptr<DicomFile>>* outFiles)
{
    RETURN_HR_IF_NULL(E_POINTER, outFiles);
    outFiles->clear();

    std::vector<std::wstring> children;
    RETURN_IF_FAILED(GetChildren(inputFolder, &children));

    std::vector<std::shared_ptr<DicomFile>> metadataFiles(children.size());
    std::transform(std::begin(children), std::end(children), std::begin(metadataFiles),
        [](const std::wstring& fileName)
        {
            std::shared_ptr<DicomFile> dicomFile;
            FAIL_FAST_IF_FAILED(MakeDicomMetadataFile(fileName, &dicomFile));
            return dicomFile;
        });

    *outFiles = metadataFiles;

    return S_OK;
}

inline float ParseFloat(const std::wstring& str)
{
    wchar_t* stopString;
    return static_cast<float>(wcstod(str.c_str(), &stopString));
}

inline std::vector<float> GetAsFloatVector(const std::wstring& str)
{
    std::wistringstream wisstream(str);
    std::vector<float> values;
    std::wstring token;
    while (std::getline(wisstream, token, L'\\'))
    {
        wchar_t* stopString;
        values.push_back(static_cast<float>(wcstod(token.c_str(), &stopString)));
    }
    return values;
}



// Sort the files by their order in the scene
inline HRESULT SortFilesInScene(std::vector<std::shared_ptr<DicomFile>>* outFiles)
{
    // Get orientation
    auto firstFile = outFiles->at(0);

    std::wstring firstImageOrientation;
    firstFile->GetAttribute(Tags::ImageOrientationPatient, &firstImageOrientation);

    // Ensure all images are oriented in same direction
    auto foundNotMatchingIt =
        std::find_if(
            std::begin(*outFiles), std::end(*outFiles),
            [&firstImageOrientation](auto dicomFile) {
                std::wstring imageOrientation;
                dicomFile->GetAttribute(Tags::ImageOrientationPatient, &imageOrientation);
                return _wcsicmp(imageOrientation.c_str(), firstImageOrientation.c_str()) != 0;
            });
    
    FAIL_FAST_IF_FALSE(foundNotMatchingIt == std::end(*outFiles));

    // Get first images position
    std::wstring firstImagePosition;
    firstFile->GetAttribute(Tags::ImagePositionPatient, &firstImagePosition);
    auto posValues = GetAsFloatVector(firstImagePosition);
    FAIL_FAST_IF_TRUE(posValues.size() != 3);

    // Construct image frame of reference to zorder files
    auto orientationValues = GetAsFloatVector(firstImageOrientation);
    FAIL_FAST_IF_TRUE(orientationValues.size() != 6);
    
    auto xaxis = DirectX::XMVectorSet(
        orientationValues[0],
        orientationValues[1],
        orientationValues[2],
        0.f);
    auto yaxis = DirectX::XMVectorSet(
        orientationValues[3],
        orientationValues[4],
        orientationValues[5],
        0.f);
    auto zaxis = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(xaxis, yaxis));
    
    const DirectX::XMFLOAT4X4 refFrameToWorld4x4(
        orientationValues[0], orientationValues[1], orientationValues[2], 0,
        orientationValues[3], orientationValues[4], orientationValues[5], 0,
        DirectX::XMVectorGetX(zaxis), DirectX::XMVectorGetY(zaxis), DirectX::XMVectorGetZ(zaxis), 0,
        posValues[0], posValues[1], posValues[2], 1
    );

    auto refFrameToWorld = DirectX::XMLoadFloat4x4(&refFrameToWorld4x4);
    DirectX::XMVECTOR determinant;
    auto worldToRefFrame = DirectX::XMMatrixInverse(&determinant, refFrameToWorld);
    
    // Sort the files by their order in the scene
    std::sort(std::begin(*outFiles), std::end(*outFiles),
        [&worldToRefFrame](auto left, auto right)
        {
            std::wstring leftImagePosition, rightImagePosition;
            left->GetAttribute(Tags::ImagePositionPatient, &leftImagePosition);
            auto leftPosValues = GetAsFloatVector(leftImagePosition);
            FAIL_FAST_IF_TRUE(leftPosValues.size() != 3);
            auto leftVec = DirectX::XMVectorSet(leftPosValues[0], leftPosValues[1], leftPosValues[2], 1.f);
            leftVec = DirectX::XMVector4Transform(leftVec, worldToRefFrame);

            right->GetAttribute(Tags::ImagePositionPatient, &rightImagePosition);
            auto rightPosValues = GetAsFloatVector(rightImagePosition);
            FAIL_FAST_IF_TRUE(rightPosValues.size() != 3);
            auto rightVec = DirectX::XMVectorSet(rightPosValues[0], rightPosValues[1], rightPosValues[2], 1.f);
            rightVec = DirectX::XMVector4Transform(rightVec, worldToRefFrame);

            return DirectX::XMVectorGetZ(leftVec) > DirectX::XMVectorGetZ(rightVec);
        });

    return S_OK;
}

template <typename... TArgs>
HRESULT Log(const wchar_t* pMessage, TArgs&&... arguments)
{
#ifdef DEBUG
    RETURN_HR_IF_NULL(E_INVALIDARG, pMessage);
    wprintf(pMessage, arguments...);
    wprintf(L"\n");
#else // DEBUG
    UNREFERENCED_PARAMETER(pMessage);
#endif
    return S_OK;

}

inline HRESULT SaveToFile(
    Application::Infrastructure::DeviceResources& resources,
    ID3D11Buffer* pBuffer,
    unsigned width,
    unsigned height,
    unsigned bytesPerPixel,
    const wchar_t* pFileName)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pBuffer);
    RETURN_HR_IF_NULL(E_INVALIDARG, pFileName);

    // Copy resource
    Microsoft::WRL::ComPtr<ID3D11Buffer> spCopy;
    RETURN_IF_FAILED(resources.GetBufferOnCPU(pBuffer, &spCopy));

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    RETURN_IF_FAILED(resources.Map(spCopy.Get(), &mappedResource));

    auto data = reinterpret_cast<float*>(mappedResource.pData);
    std::ofstream stream(pFileName, std::ios_base::trunc | std::ios_base::binary | std::ios_base::out);

    stream.write(reinterpret_cast<const char*>(&width), sizeof(unsigned));
    stream.write(reinterpret_cast<const char*>(&height), sizeof(unsigned));
    stream.write(reinterpret_cast<const char*>(&bytesPerPixel), sizeof(unsigned));
    stream.write(reinterpret_cast<const char*>(data), width * height * bytesPerPixel);

    return S_OK;
}


inline HRESULT SaveToFile(
    Application::Infrastructure::DeviceResources& resources,
    ID3D11Buffer* pBuffer,
    unsigned width,
    unsigned height,
    unsigned bytesPerPixel,
    WICPixelFormatGUID& format,
    const wchar_t* pFileName)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pBuffer);
    RETURN_HR_IF_NULL(E_INVALIDARG, pFileName);

    // Copy resource
    Microsoft::WRL::ComPtr<ID3D11Buffer> spCopy;
    RETURN_IF_FAILED(resources.GetBufferOnCPU(pBuffer, &spCopy));

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    RETURN_IF_FAILED(resources.Map(spCopy.Get(), &mappedResource));

    // Set a break point here and put down the expression "p, 1024" in your watch window to see what has been written out by our CS
    // This is also a common trick to debug CS programs.
    Microsoft::WRL::ComPtr<IWICBitmap> spBitmap;
    RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateBitmapFromMemory(
        width,
        height,
        format,
        bytesPerPixel * width,
        bytesPerPixel * width * height,
        reinterpret_cast<BYTE*>(mappedResource.pData),
        &spBitmap));

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> spEncoder;
    RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &spEncoder));

    Microsoft::WRL::ComPtr<IWICStream> spStream;

    RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateStream(&spStream));

    RETURN_IF_FAILED(spStream->InitializeFromFilename(pFileName, GENERIC_WRITE));
    RETURN_IF_FAILED(spEncoder->Initialize(spStream.Get(), WICBitmapEncoderNoCache));

    Microsoft::WRL::ComPtr<IWICMetadataBlockWriter> spBlockWriter;

    // Frame variables
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> spFrameEncode;

    // Get and create image frame
    RETURN_IF_FAILED(spEncoder->CreateNewFrame(&spFrameEncode, nullptr));

    //Initialize the encoder
    RETURN_IF_FAILED(spFrameEncode->Initialize(nullptr));
    RETURN_IF_FAILED(spFrameEncode->SetSize(width, height));

    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormatDontCare;
    RETURN_IF_FAILED(spFrameEncode->SetPixelFormat(&pixelFormat));

    // Copy updated bitmap to output
    RETURN_IF_FAILED(spFrameEncode->WriteSource(spBitmap.Get(), nullptr));
    
    //Commit the frame
    RETURN_IF_FAILED(spFrameEncode->Commit());
    RETURN_IF_FAILED(spEncoder->Commit());

    RETURN_IF_FAILED(resources.Unmap(spCopy.Get()));

    return S_OK;
}

inline HRESULT GetWicBitmapFromFilename(
    Application::Infrastructure::DeviceResources& resources,
    const wchar_t* pwzInputFile,
    IWICBitmapSource** ppSource)
{
    RETURN_HR_IF_NULL(E_POINTER, ppSource);

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> spDecoder;
    RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateDecoderFromFilename(
        pwzInputFile,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &spDecoder
    ));

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> spDecodedFrame;
    RETURN_IF_FAILED(spDecoder->GetFrame(0, &spDecodedFrame));

    Microsoft::WRL::ComPtr<IWICFormatConverter> spConverter;
    RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateFormatConverter(&spConverter));

    RETURN_IF_FAILED(spConverter->Initialize(
        spDecodedFrame.Get(),
        GUID_WICPixelFormat32bppGrayFloat,
        WICBitmapDitherTypeNone,
        nullptr,
        0.f,
        WICBitmapPaletteTypeCustom));

    RETURN_IF_FAILED(spConverter.CopyTo(ppSource));
    return S_OK;
}

template <typename T>
HRESULT GetBufferFromGrayscaleDicomData(
    const wchar_t* pwzInputFile,
    std::vector<T>* pData,
    unsigned* pWidth,
    unsigned* pHeight,
    unsigned* pChannels)
{
    unsigned bytesPerPixel;
    std::ifstream stream(pwzInputFile, std::ios_base::binary);

    stream.read(reinterpret_cast<char*>(pWidth), sizeof(unsigned));
    stream.read(reinterpret_cast<char*>(pHeight), sizeof(unsigned));
    stream.read(reinterpret_cast<char*>(&bytesPerPixel), sizeof(unsigned));

    RETURN_HR_IF_FALSE(E_FAIL, bytesPerPixel == sizeof(T));
    *pChannels = 1;

    unsigned stride = bytesPerPixel * (*pWidth);
    unsigned bufferSize = stride * (*pHeight);
    pData->resize((*pWidth) * (*pHeight));
    stream.read(reinterpret_cast<char *>(&pData->at(0)), bufferSize);
    return S_OK;
}

template <typename T>
HRESULT GetBufferFromGrayscaleImage(
    Application::Infrastructure::DeviceResources& resources,
    const wchar_t* pwzInputFile,
    std::vector<T>* pData,
    unsigned* pWidth,
    unsigned* pHeight,
    unsigned* pChannels)
{
    RETURN_HR_IF_NULL(E_FAIL, pData);
    RETURN_HR_IF_NULL(E_FAIL, pWidth);
    RETURN_HR_IF_NULL(E_FAIL, pHeight);
    RETURN_HR_IF_NULL(E_FAIL, pChannels);

    std::wstring fileName(pwzInputFile);
    if (fileName.rfind(L".dd") + 3 == fileName.size())
    {
        return GetBufferFromGrayscaleDicomData(pwzInputFile, pData, pWidth, pHeight, pChannels);
    }

    Microsoft::WRL::ComPtr<IWICBitmapSource> spSource;
    RETURN_IF_FAILED(GetWicBitmapFromFilename(resources, pwzInputFile, &spSource));
    RETURN_IF_FAILED(spSource->GetSize(pWidth, pHeight));

    IID clsid;
    spSource->GetPixelFormat(&clsid);
    Microsoft::WRL::ComPtr<IWICComponentInfo> spComponentInfo;
    RETURN_IF_FAILED(resources.GetWicImagingFactory()->CreateComponentInfo(clsid, &spComponentInfo));

    Microsoft::WRL::ComPtr<IWICPixelFormatInfo> spWICPixelFormatInfo;
    RETURN_IF_FAILED(spComponentInfo->QueryInterface(IID_PPV_ARGS(&spWICPixelFormatInfo)));

    unsigned bpp;
    RETURN_IF_FAILED(spWICPixelFormatInfo->GetBitsPerPixel(&bpp));
    unsigned bytesPerPixel = bpp / 8;
    RETURN_HR_IF_FALSE(E_FAIL, bytesPerPixel == sizeof(T));

    RETURN_IF_FAILED(spWICPixelFormatInfo->GetChannelCount(pChannels));
    RETURN_HR_IF_FALSE(E_FAIL, *pChannels == 1);

    unsigned stride = bytesPerPixel * (*pWidth);
    unsigned bufferSize = stride * (*pHeight);
    pData->resize((*pChannels) * (*pWidth) * (*pHeight));
    RETURN_IF_FAILED(spSource->CopyPixels(nullptr, stride, bufferSize, reinterpret_cast<unsigned char*>(&pData->at(0))));

    return S_OK;
}

inline HRESULT CreateShader(
    Application::Infrastructure::DeviceResources& resources,
    std::wstring shaderFile,
    ID3D11ComputeShader** ppOutShader)
{
    UNREFERENCED_PARAMETER(resources);
    wchar_t pwzFileName[MAX_PATH + 1];
    RETURN_HR_IF(E_FAIL, 0 == GetModuleFileName(NULL, pwzFileName, MAX_PATH + 1));
    std::wstring shaderPath(pwzFileName);
    shaderPath.erase(shaderPath.begin() + shaderPath.find_last_of(L'\\') + 1, shaderPath.end());
    shaderPath += shaderFile;

    RETURN_IF_FAILED(resources.CreateComputeShader(shaderPath.c_str(), "CSMain", ppOutShader));
    return S_OK;
}


}