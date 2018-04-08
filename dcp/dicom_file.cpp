#include "precomp.h"
#include "dicom_file.h"

using namespace DCM;


template <unsigned TBytesRead>
HRESULT Discard(std::istream& stream)
{
    stream.ignore(TBytesRead);
    RETURN_HR_IF(E_FAIL, stream.good());
    return S_OK;
}


template <typename T>
HRESULT Read(std::istream& stream, _Out_ T* group)
{
	RETURN_HR_IF_NULL(E_FAIL, group);
    stream.read(reinterpret_cast<char*>(group), sizeof(T));
    RETURN_HR_IF(E_FAIL, stream.good());
    return S_OK;
}

DicomFile::DicomFile(
    const std::wstring& fileName,
    const std::vector<DicomTag>& tags) :
        m_stream(fileName, std::ifstream::binary),
		m_tags(tags)
{
	Load();
}

_Use_decl_annotations_
HRESULT DicomFile::TagToId(const DicomTag& tag, unsigned* id)
{
	RETURN_HR_IF_NULL(E_FAIL, id);
	*id = (tag.Group << sizeof(WORD) * 8) + tag.Element;
	return S_OK;
}

_Use_decl_annotations_
HRESULT DicomFile::GetAttribute(const DicomTag& tag, std::vector<char>* data)
{
    RETURN_HR_IF_NULL(E_POINTER, data);
	unsigned id;
    RETURN_IF_FAILED(TagToId(tag, &id));
    auto foundIt = m_Attributes.find(id);
    RETURN_HR_IF(E_FAIL, foundIt == std::end(m_Attributes));
    *data = m_Attributes[id];
    return E_FAIL;
}

_Use_decl_annotations_
HRESULT DicomFile::IsMappedTag(const DicomTag& tag, bool* isMapped)
{
	RETURN_HR_IF_NULL(E_FAIL, isMapped);
	*isMapped = false;
	for (auto attribute : m_tags)
	{
		bool groupMatched = attribute.Group == tag.Group;
		bool elementMatched = attribute.Element == tag.Element;

		*isMapped = groupMatched && elementMatched;

        if (*isMapped)
        {
            break;
        }
	}
	return S_OK;
}

#include <sstream>
#include <iomanip>
#include <stack>
HRESULT DicomFile::Load()
{
	RETURN_IF_FAILED(Discard<128>(m_stream));

	DicomPreamble preamble;
	RETURN_IF_FAILED(Read(m_stream, &preamble));

	std::stack<DWORD> remainingBytesInSequance;
	while (!m_stream.eof())
	{
		DicomAttribute attribute;
		RETURN_IF_FAILED(Read(m_stream, &attribute.Tag));

		if (attribute.Tag.Group == 0xFFFE && attribute.Tag.Element == 0xE0DD || // sequence end marker
			attribute.Tag.Group == 0xFFFE && attribute.Tag.Element == 0xE000 || // item begin marker
			attribute.Tag.Group == 0xFFFE && attribute.Tag.Element == 0xE00D)   // item end marker
		{
			// This is a data item begin/end marker, and there is no VR
			attribute.ValueRepresentation[0] = '\0';
			attribute.ValueRepresentation[0] = '\0';
		}
		else
		{
			RETURN_IF_FAILED(Read(m_stream, &attribute.ValueRepresentation));
		}

		DWORD ValueLength;
		if (_strnicmp((char*)attribute.ValueRepresentation, "\0\0", 2) == 0)
		{
			RETURN_IF_FAILED(Read(m_stream, &ValueLength));
		}
		else if (_strnicmp((char*)attribute.ValueRepresentation, "OB", 2) == 0 ||
			_strnicmp((char*)attribute.ValueRepresentation, "OW", 2) == 0 ||
			_strnicmp((char*)attribute.ValueRepresentation, "OF", 2) == 0 ||
			_strnicmp((char*)attribute.ValueRepresentation, "SQ", 2) == 0 ||
			_strnicmp((char*)attribute.ValueRepresentation, "UT", 2) == 0 ||
			_strnicmp((char*)attribute.ValueRepresentation, "UN", 2) == 0)
		{
			Discard<2>(m_stream);
			RETURN_IF_FAILED(Read(m_stream, &ValueLength));
		}
		else
		{
			WORD sValueLength;
			RETURN_IF_FAILED(Read(m_stream, &sValueLength));
			ValueLength = sValueLength;
		}

		if (_strnicmp((char*)attribute.ValueRepresentation, "SQ", 2) == 0 ||    // sequence begin marker
		    attribute.Tag.Group == 0xFFFE && attribute.Tag.Element == 0xE0DD || // sequence end marker
			attribute.Tag.Group == 0xFFFE && attribute.Tag.Element == 0xE000 || // item begin marker
			attribute.Tag.Group == 0xFFFE && attribute.Tag.Element == 0xE00D)   // item end marker
		{
			// In theory we should be tracking whick sequence we are a part of
			// and what ordinal we are at... this may me irrelevant though...
			continue;
		}

        std::vector<char> buffer(ValueLength);
		m_stream.read(&buffer[0], ValueLength);

		bool isMapped;
		unsigned id;
		if (SUCCEEDED(IsMappedTag(attribute.Tag, &isMapped)) &&
			SUCCEEDED(TagToId(attribute.Tag, &id)) &&
			isMapped)
		{
            m_Attributes.emplace(id, std::move(buffer));
		}
	}

	return S_OK;
}