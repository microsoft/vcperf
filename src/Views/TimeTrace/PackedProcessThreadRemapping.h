#pragma once

#include <unordered_map>

#include "Analyzers\ExecutionHierarchy.h"

namespace vcperf
{

class PackedProcessThreadRemapping
{
public:

    struct Remap
    {
        unsigned long ProcessId = 0UL;
        unsigned long ThreadId = 0UL;
    };

public:

    PackedProcessThreadRemapping();

    void Calculate(const ExecutionHierarchy* hierarchy);
    void CalculateChildrenLocalThreadData(const ExecutionHierarchy::Entry* entry);

    const Remap* GetRemapFor(unsigned long long id) const;

private:

    struct LocalOffsetData
    {
        unsigned long RawLocalThreadId = 0UL;
        unsigned long RequiredThreadIdToFitHierarchy = 0UL;
        unsigned long CalculatedLocalThreadId = 0UL;
    };

    void RemapRootsProcessId(const ExecutionHierarchy* hierarchy);
    void RemapEntriesThreadId(const ExecutionHierarchy* hierarchy);
    void RemapThreadIdFor(const ExecutionHierarchy::Entry* entry, unsigned long remappedProcessId,
                          unsigned long parentAbsoluteThreadId);

    void CalculateChildrenLocalThreadId(const ExecutionHierarchy::Entry* entry);
    void CalculateChildrenExtraThreadIdToFitHierarchy(const ExecutionHierarchy::Entry* entry);

    std::unordered_map<unsigned long long, Remap> remappings_;
    std::unordered_map<unsigned long long, LocalOffsetData> localOffsetsData_;
};

} // namespace vcperf
