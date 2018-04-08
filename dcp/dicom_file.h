//
// DicomFile.h
// Declaration of the App class.
//

#pragma once

#include <istream>
#include <fstream>


namespace DCM
{
	struct DicomTag
	{
		WORD Group;
		WORD Element;
	};

	struct DicomAttribute
	{
		DicomTag Tag;
		char ValueRepresentation[2];
	};

	struct DicomPreamble
	{
		char Code[4];
	};

namespace Tags
{
    __declspec(selectany) DicomTag SamplesPerPixel = { 0x0028, 0x0002 };
    __declspec(selectany) DicomTag BitsAllocated = { 0x0028, 0x0100 };
    __declspec(selectany) DicomTag Rows = { 0x0028, 0x0010 };
    __declspec(selectany) DicomTag Columns = { 0x0028, 0x0011 };
    __declspec(selectany) DicomTag PixelData = { 0x7FE0, 0x0010 };
    __declspec(selectany) DicomTag SliceThickness = { 0x0018, 0x0050 };
    __declspec(selectany) DicomTag WindowCenter = { 0x0028, 0x1050 };
    __declspec(selectany) DicomTag WindowWidth = { 0x0028, 0x1051 };
    __declspec(selectany) DicomTag PixelSpacing = { 0x0028, 0x0030 };
    __declspec(selectany) DicomTag ImagePositionPatient = { 0x0020, 0x0032 };
    __declspec(selectany) DicomTag ImageOrientationPatient = { 0x0020, 0x0037 };
    __declspec(selectany) DicomTag PatientPosition = { 0x0018, 0x5100 };
}
	/// <summary>
	/// Provides application-specific behavior to supplement the default Application class.
	/// </summary>
	class DicomFile 
	{
	public:
		DicomFile(
            const std::wstring& fileName,
			const std::vector<DicomTag>& tags);

		HRESULT GetAttribute(const DicomTag& tag, std::vector<char>* data);

        template <typename TOut>
        HRESULT GetAttributeAs(std::vector<char> buffer, _Out_ TOut* out)
        {
            RETURN_HR_IF_NULL(E_FAIL, out);
            memcpy(out, &buffer[0], buffer.size());
            return S_OK;
        }

	private:
		HRESULT Load();

		HRESULT IsMappedTag(const DicomTag& tag, _Out_ bool* isMapped);
		HRESULT TagToId(const DicomTag& tag, _Out_ unsigned* id);

		std::ifstream m_stream;
		std::vector<DicomTag> m_tags;

		std::map<unsigned, std::vector<char>> m_Attributes;
	};

    static HRESULT MakeDicomImageFile(const std::wstring& path, std::shared_ptr<DicomFile>* pFile)
    {
        RETURN_HR_IF_NULL(E_POINTER, pFile);
        static const std::vector<DicomTag> tags =
        {
            Tags::SamplesPerPixel,
            Tags::BitsAllocated,
            Tags::Rows,
            Tags::Columns,
            Tags::PixelData,
            Tags::SliceThickness,
            Tags::WindowWidth,
            Tags::WindowCenter,
            Tags::PixelSpacing,
            Tags::ImagePositionPatient,
            Tags::ImageOrientationPatient,
            Tags::PatientPosition
        };
        *pFile = std::make_shared<DicomFile>(path, tags);
        return S_OK;
    }

}