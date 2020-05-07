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
        std::string Name;

        std::vector<Entry*> Children;

        bool OverlapsWith(const Entry* other) const;
    };

    typedef std::vector<const Entry*> TRoots;

public:

    ExecutionHierarchy();

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;

    const Entry* GetEntry(unsigned long long id) const;
    inline const TRoots& GetRoots() const { return roots_; }

private:

    void OnRootActivity(const A::Activity& root);
    void OnNestedActivity(const A::Activity& parent, const A::Activity& child);
    void OnFinishActivity(const A::Activity& activity);

    Entry* CreateEntry(const A::Activity& activity);

    std::chrono::nanoseconds ConvertTime(long long ticks, long long frequency) const;

    std::unordered_map<unsigned long long, Entry> entries_;
    TRoots roots_;
};

} // namespace vcperf
