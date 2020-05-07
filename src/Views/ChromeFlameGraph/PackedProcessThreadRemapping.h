#pragma once

#include <unordered_map>

namespace vcperf
{

class ExecutionHierarchy;

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

    void Calculate(const ExecutionHierarchy* hierarchy);

    const Remap* GetRemapFor(unsigned long long id) const;

private:

    void RemapRootsProcessId(const ExecutionHierarchy* hierarchy);
    void RemapEntriesThreadId(const ExecutionHierarchy* hierarchy);

    std::unordered_map<unsigned long long, Remap> remappings_;
};

} // namespace vcperf
