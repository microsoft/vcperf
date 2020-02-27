#pragma once

namespace vcperf
{

struct Guid : GUID
{
    constexpr Guid(
        unsigned long  d1, 
        unsigned short d2,
        unsigned short d3,
        unsigned char b1,
        unsigned char b2,
        unsigned char b3,
        unsigned char b4,
        unsigned char b5,
        unsigned char b6,
        unsigned char b7,
        unsigned char b8
    ):
        GUID{d1, d2, d3, b1, b2, b3, b4, b5, b6, b7, b8}
    {
    }
};

// f78a07b0-796a-5da4-5c20-61aa526e77af
constexpr Guid CppBuildInsightsGuid = Guid{0xf78a07b0, 0x796a, 0x5da4,
    0x5c, 0x20, 0x61, 0xaa, 0x52, 0x6e, 0x77, 0xaf};

} // namespace vcperf




