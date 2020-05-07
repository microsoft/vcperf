#pragma once

namespace vcperf
{

class PackedProcessThreadRemapping
{
public:

    struct Remap
    {
        unsigned long ProcessId;
        unsigned long ThreadId;
    };

public:

    PackedProcessThreadRemapping();

    const Remap* GetRemapFor(unsigned long long id) const;
};

} // namespace vcperf
