#pragma once

#include <unordered_map>
#include <vector>
#include <chrono>

#include "VcperfBuildInsights.h"

namespace vcperf
{

class ExecutionHierarchy : public BI::IAnalyzer
{
public:

    struct Entry
    {
        unsigned long long Id;
        unsigned long ProcessId;
        unsigned long ThreadId;
        std::chrono::nanoseconds StartTimestamp;
        std::chrono::nanoseconds StopTimestamp;
        std::wstring Name;

        std::vector<Entry*> Children;
    };

public:

    ExecutionHierarchy();

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;

private:

    void OnRootActivity(const A::Activity& root);
    void OnNestedActivity(const A::Activity& parent, const A::Activity& child);
    void OnFinishActivity(const A::Activity& activity);

    Entry* CreateEntry(const A::Activity& activity);

    std::chrono::nanoseconds ConvertTime(long long ticks, long long frequency) const;

    std::unordered_map<unsigned long long, Entry> entries_;
    std::vector<Entry*> roots_;
};

} // namespace vcperf
