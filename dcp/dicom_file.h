//
// DicomFile.h
// Declaration of the App class.
//

#pragma once

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
    __declspec(selectany) DicomTag NumberOfSeriesRelatedInstances = { 0x0020, 0x1209 };
    __declspec(selectany) DicomTag NumberOfStudyRelatedSeries = { 0x0020, 0x1206 };
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
        HRESULT GetAttribute(const DicomTag& tag, _Out_ std::wstring* out)
        {
            RETURN_HR_IF_NULL(E_FAIL, out);
            std::vector<char> buffer;
            RETURN_IF_FAILED(GetAttribute(tag, &buffer));

            std::string bufferAsString(std::begin(buffer), std::end(buffer));
            out->resize(bufferAsString.size());
            std::copy(std::begin(bufferAsString), std::end(bufferAsString), std::begin(*out));
            return S_OK;
        }

        template <typename T>
        HRESULT GetAttribute(const DicomTag& tag, _Out_ T* out)
        {
            RETURN_HR_IF_NULL(E_FAIL, out);
            std::vector<char> buffer;
            RETURN_IF_FAILED(GetAttribute(tag, &buffer));

            if (buffer.size() == 2)
            {
                *out = static_cast<T>(*reinterpret_cast<unsigned short*>(&buffer[0]));
                return S_OK;
            }
            else if (buffer.size() == 4)
            {
                *out = static_cast<T>(*reinterpret_cast<unsigned long*>(&buffer[0]));
                return S_OK;
            }
            return E_FAIL;
        }

        HRESULT GetAttributeReference(const DicomTag& tag, std::vector<char>** data);

        template <typename TOut>
        HRESULT GetAttributeAs(std::vector<char> buffer, _Out_ TOut* out)
        {
            RETURN_HR_IF_NULL(E_FAIL, out);
            memcpy(out, &buffer[0], buffer.size());
            return S_OK;
        }

        const std::wstring& SafeGetFilename()
        {
            return m_fileName;
        }

	private:
		HRESULT Load();

		HRESULT IsMappedTag(const DicomTag& tag, _Out_ bool* isMapped);
		HRESULT TagToId(const DicomTag& tag, _Out_ unsigned* id);

        std::wstring m_fileName;
		std::ifstream m_stream;
		std::vector<DicomTag> m_tags;

		std::map<unsigned, std::vector<char>> m_Attributes;
	};

    inline HRESULT MakeDicomMetadataFile(const std::wstring& path, std::shared_ptr<DicomFile>* pFile)
    {
        RETURN_HR_IF_NULL(E_POINTER, pFile);
        static const std::vector<DicomTag> tags =
        {
            Tags::SamplesPerPixel,
            Tags::BitsAllocated,
            Tags::Rows,
            Tags::Columns,
            Tags::SliceThickness,
            Tags::WindowWidth,
            Tags::WindowCenter,
            Tags::PixelSpacing,
            Tags::ImagePositionPatient,
            Tags::ImageOrientationPatient,
            Tags::PatientPosition,
            Tags::NumberOfSeriesRelatedInstances,
            Tags::NumberOfStudyRelatedSeries
        };
        *pFile = std::make_shared<DicomFile>(path, tags);
        return S_OK;
    }

    inline HRESULT MakeDicomImageFile(const std::wstring& path, std::shared_ptr<DicomFile>* pFile)
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
            Tags::PatientPosition,
            Tags::NumberOfSeriesRelatedInstances,
            Tags::NumberOfStudyRelatedSeries
        };
        *pFile = std::make_shared<DicomFile>(path, tags);
        return S_OK;
    }

}