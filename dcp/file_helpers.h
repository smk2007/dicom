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
            wprintf(L"%ls \n", path.c_str());
            children->push_back(std::move(path));
        }
        else if (type == FileType::File &&
                (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            wprintf(L"%ls \n", path.c_str());
            children->push_back(std::move(path));
        }
    }

    return S_OK;
}
}