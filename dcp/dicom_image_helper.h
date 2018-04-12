//
// dicom_image_helper.h
//

#pragma once

namespace DCM
{

enum class ImageProperty
{
    BitsAllocated,
    BytesAllocated,
    Columns,
    Length,
    Pitch,
    PixelData,
    Rows,
    SamplesPerPixel,
    Spacings,
    WindowCenter,
    WindowRange,
};

template <ImageProperty TProperty> struct PropertyToTagMapper;
template <> struct PropertyToTagMapper<ImageProperty::BitsAllocated> { static const DicomTag Tag; };
__declspec(selectany) const DicomTag PropertyToTagMapper<ImageProperty::BitsAllocated>::Tag = Tags::BitsAllocated;
template <> struct PropertyToTagMapper<ImageProperty::Columns> { static const DicomTag Tag; };
__declspec(selectany) const DicomTag PropertyToTagMapper<ImageProperty::Columns>::Tag = Tags::Columns;
template <> struct PropertyToTagMapper<ImageProperty::PixelData> { static const DicomTag Tag; };
__declspec(selectany) const DicomTag PropertyToTagMapper<ImageProperty::PixelData>::Tag = Tags::PixelData;
template <> struct PropertyToTagMapper<ImageProperty::Rows> { static const DicomTag Tag; };
__declspec(selectany) const DicomTag PropertyToTagMapper<ImageProperty::Rows>::Tag = Tags::Rows;
template <> struct PropertyToTagMapper<ImageProperty::SamplesPerPixel> { static const DicomTag Tag; };
__declspec(selectany) const DicomTag PropertyToTagMapper<ImageProperty::SamplesPerPixel>::Tag = Tags::SamplesPerPixel;
template <> struct PropertyToTagMapper<ImageProperty::WindowCenter> { static const DicomTag Tag; };
__declspec(selectany) const DicomTag PropertyToTagMapper<ImageProperty::WindowCenter>::Tag = Tags::WindowCenter;
template <> struct PropertyToTagMapper<ImageProperty::WindowRange> { static const DicomTag Tag; };
__declspec(selectany) const DicomTag PropertyToTagMapper<ImageProperty::WindowRange>::Tag = Tags::WindowWidth;

template <ImageProperty TProperty> struct Property
{
    template <typename T = float>
    static T SafeGet(std::shared_ptr<DicomFile> file)
    {
        T value;
        FAIL_FAST_IF_FAILED(file->GetAttribute(PropertyToTagMapper<TProperty>::Tag, &value));
        return value;
    }
};

template <> struct Property<ImageProperty::Spacings>
{
    template <typename T = float>
    static std::vector<T> SafeGet(std::shared_ptr<DicomFile> file)
    {
        // Spatial spacing
        std::wstring pixelSpacing;
        FAIL_FAST_IF_FAILED(file->GetAttribute(DCM::Tags::PixelSpacing, &pixelSpacing));
        auto spacings = GetAsFloatVector(pixelSpacing);
        std::vector<T> outSpacings(3);
        outSpacings[0] = static_cast<T>(spacings[0]);
        outSpacings[1] = static_cast<T>(spacings[1]);

        std::wstring sliceThickness;
        FAIL_FAST_IF_FAILED(file->GetAttribute(DCM::Tags::SliceThickness, &sliceThickness));
        outSpacings[2] = static_cast<T>(ParseFloat(sliceThickness));
        return outSpacings;
    }
};

template <> struct Property<ImageProperty::PixelData>
{
    static std::vector<char>* SafeGet(std::shared_ptr<DicomFile> file)
    {
        std::vector<char>* value;
        FAIL_FAST_IF_FAILED(file->GetAttributeReference(Tags::PixelData, &value));
        return value;
    }
};

template <> struct Property<ImageProperty::BytesAllocated>
{
    template <typename T = float>
    static T SafeGet(std::shared_ptr<DicomFile> file)
    {
        return Property<ImageProperty::BitsAllocated>::SafeGet<T>(file) / 8;
    }
};

template <> struct Property<ImageProperty::Pitch>
{
    template <typename T = float>
    static T SafeGet(std::shared_ptr<DicomFile> file)
    {
        return
            Property<ImageProperty::SamplesPerPixel>::SafeGet<T>(file) *
            Property<ImageProperty::Columns>::SafeGet<T>(file) *
            Property<ImageProperty::BytesAllocated>::SafeGet<T>(file);
    }
};

template <> struct Property<ImageProperty::Length>
{
    template <typename T = float>
    static T SafeGet(std::shared_ptr<DicomFile> file)
    {
        return Property<ImageProperty::Pitch>::SafeGet<T>(file) *
               Property<ImageProperty::Rows>::SafeGet<T>(file);
    }
};
} // DCM