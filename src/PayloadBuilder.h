#pragma once
#include <stdint.h>
#include <string>
#include <array>
#include <numeric>

namespace vcperf
{

class Payload
{
public:
    Payload():
        payloadData{nullptr},
        payloadByteSize{0}
    {}

    Payload(uint8_t* mem, size_t byteSize) :
        payloadData(mem), payloadByteSize(byteSize) {

    }

    ~Payload() {
        free(payloadData);
    }

    Payload(const Payload&) = delete;
    Payload& operator=(const Payload&) = delete;

    Payload& operator=(Payload&& rhs) noexcept
    {
        payloadData = rhs.payloadData;
        payloadByteSize = rhs.payloadByteSize;

        rhs.payloadByteSize = 0;
        rhs.payloadData = nullptr;

        return *this;
    }

    uint8_t* GetData() { return payloadData; }
    size_t Size() const { return payloadByteSize; }

private:
    uint8_t* payloadData;
    size_t payloadByteSize;
};

template <typename... Fields>
class PayloadBuilder
{
    typedef std::array<size_t, sizeof...(Fields)> FieldSizeArray;

public:

    static Payload Build(Fields... fields) {

        FieldSizeArray fieldSizes;

        ComputeFieldSizes<0>(fieldSizes, fields...);

        size_t totalSize = std::accumulate(begin(fieldSizes), end(fieldSizes), size_t{});

        if (totalSize == 0) {
            return { nullptr, 0 };
        }

        uint8_t* mem = static_cast<uint8_t*>(malloc(totalSize));

        CopyFields<0>(mem, fieldSizes, fields...);

        return { mem, totalSize };
    }

private:

    template <unsigned fieldIndex, typename Field, typename... OtherFields>
    static void ComputeFieldSizes(FieldSizeArray& sizes, Field field, OtherFields... otherFields) {

        static_assert(std::is_integral_v<Field> || std::is_floating_point_v<Field>, 
            "Only integral, floating-point, and pointer types can be used for payload fields.");

        sizes[fieldIndex] = sizeof(Field);

        ComputeFieldSizes<fieldIndex + 1>(sizes, otherFields...);
    }

    template <unsigned fieldIndex, typename Field, typename... OtherFields>
    static void ComputeFieldSizes(FieldSizeArray& sizes, Field* field, OtherFields... otherFields) {

        sizes[fieldIndex] = ComputePointerFieldSize(field);

        ComputeFieldSizes<fieldIndex + 1>(sizes, otherFields...);
    }

    template <unsigned fieldIndex>
    static void ComputeFieldSizes(FieldSizeArray& sizes) {
    }

    static size_t ComputePointerFieldSize(const wchar_t* wideString) {
        return (wcslen(wideString) + 1) * sizeof(wchar_t);
    }

    static size_t ComputePointerFieldSize(const char* string) {
        return (strlen(string) + 1) * sizeof(char);
    }


    template <unsigned fieldIndex, typename Field, typename... OtherFields>
    static void CopyFields(uint8_t* dataPtr, FieldSizeArray& sizes, Field field, OtherFields... otherFields) {

        static_assert(std::is_integral_v<Field> || std::is_floating_point_v<Field>, 
            "Only integral, floating-point, and pointer types can be used for payload fields.");

        memcpy(dataPtr, &field, sizes[fieldIndex]);

        CopyFields<fieldIndex + 1>(dataPtr + sizes[fieldIndex], sizes, otherFields...);
    }

    template <unsigned fieldIndex, typename Field, typename... OtherFields>
    static void CopyFields(uint8_t* dataPtr, FieldSizeArray& sizes, Field* field, OtherFields... otherFields) {

        memcpy(dataPtr, field, sizes[fieldIndex]);

        CopyFields<fieldIndex + 1>(dataPtr + sizes[fieldIndex], sizes, otherFields...);
    }

    template <unsigned fieldIndex>
    static void CopyFields(uint8_t* dataPtr, FieldSizeArray& sizes) {
    }
};

} // namespace vcperf