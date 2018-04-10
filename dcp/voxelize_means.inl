#pragma once

template <OperationType TType>
struct VoxelizeOperation
{
    unsigned m_xInMillimeters;
    unsigned m_yInMillimeters;
    unsigned m_zInMillimeters;

    std::wstring m_inputFolder;

    VoxelizeOperation(
        const std::wstring& inputFolder,
        unsigned xInMillimeters,
        unsigned yInMillimeters,
        unsigned zInMillimeters) :
            m_inputFolder(inputFolder),
            m_xInMillimeters(xInMillimeters),
            m_yInMillimeters(yInMillimeters),
            m_zInMillimeters(zInMillimeters)
    {}

    HRESULT Run(Application::Infrastructure::DeviceResources& resources)
    {
        Concurrency::ConcurrentQueue<std::shared_ptr<DicomFile>> dataFiles(100);

        std::thread t1(
            [](auto inputFolder, auto files)
            {
                [&]()->HRESULT
                {
                    std::vector<std::shared_ptr<DicomFile>> metadataFiles;
                    RETURN_IF_FAILED(GetMetadataFiles(inputFolder, &metadataFiles));
                    RETURN_IF_FAILED(SortFilesInScene(&metadataFiles));

                    for (auto file : metadataFiles)
                    {
                        const wchar_t* pName;
                        file->GetFilename(&pName);

                        // Get the full file
                        std::shared_ptr<DicomFile> fullFile;
                        FAIL_FAST_IF_FAILED(MakeDicomImageFile(std::wstring(pName), &fullFile));
                        RETURN_IF_FAILED(files.get().Enqueue(std::move(fullFile)));
                    }

                    files.get().Finish();
                    return S_OK;
                }();
            }, m_inputFolder, std::ref(dataFiles));

        std::thread t2(
            [](auto resources, auto dicomFiles)
            {
                [&]()->HRESULT
                {
                    bool isDefunct;
                    unsigned i = 0;
                    while (SUCCEEDED(dicomFiles.get().IsDefunct(&isDefunct)) && !isDefunct)
                    {
                        std::shared_ptr<DicomFile> file;
                        RETURN_IF_FAILED(dicomFiles.get().Dequeue(&file));
                        const wchar_t* pName;
                        file->GetFilename(&pName);
                        wprintf(L"Processing %d: %ls \n", i++, pName);

                        auto data = Property<ImageProperty::PixelData>::SafeGet(file);

                        auto a = Property<ImageProperty::Columns>::SafeGet<unsigned>(file);
                            auto b = Property<ImageProperty::Rows>::SafeGet<unsigned>(file);

                            auto c = Property<ImageProperty::Pitch>::SafeGet<unsigned>(file);
                            auto d = Property<ImageProperty::Length>::SafeGet<unsigned>(file);


                        Microsoft::WRL::ComPtr<IWICBitmap> spWicBitmap;
                        auto hr = (resources.get().GetWicImagingFactory()->CreateBitmapFromMemory(
                            Property<ImageProperty::Columns>::SafeGet<unsigned>(file),
                            Property<ImageProperty::Rows>::SafeGet<unsigned>(file),
                            GUID_WICPixelFormat16bppGray,
                            Property<ImageProperty::Pitch>::SafeGet<unsigned>(file),
                            Property<ImageProperty::Length>::SafeGet<unsigned>(file),
                            reinterpret_cast<BYTE*>(&(data->at(0))),
                            &spWicBitmap
                        ));

                        unsigned w, h;
                        spWicBitmap->GetSize(&w, &h);

                    }

                    return S_OK;
                }();
            }, std::ref(resources), std::ref(dataFiles));



        t1.join();
        t2.join();

        return S_OK;
    }
};