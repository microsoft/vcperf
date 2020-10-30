#pragma once

#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <cassert>

#include "VcperfBuildInsights.h"

namespace vcperf
{

class MiscellaneousCache : public BI::IAnalyzer
{
public:
    MiscellaneousCache():
        pass_{0},
        exclusivityLeaves_{}
    {}

    struct TimingData
    {
        std::chrono::nanoseconds Duration;
        std::chrono::nanoseconds ExclusiveDuration;
        std::chrono::nanoseconds CPUTime;
        std::chrono::nanoseconds ExclusiveCPUTime;
        std::chrono::nanoseconds WallClockTimeResponsibility;
    };
    
    const TimingData& GetTimingData(const A::Activity& a)
    {
        return timingData_[a.EventInstanceId()];
    }

    BI::AnalysisControl OnEndAnalysisPass() override
    {
        ++pass_;

        return BI::AnalysisControl::CONTINUE;
    }

    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override
    {
        if (pass_) {
            return BI::AnalysisControl::CONTINUE;
        }

        A::Activity a{eventStack.Back()};

        assert(timingData_.find(a.EventInstanceId()) == timingData_.end());

        auto& t = timingData_[a.EventInstanceId()];

        t.Duration = a.Duration();
        t.CPUTime = a.CPUTime();
        t.WallClockTimeResponsibility = a.WallClockTimeResponsibility();

        auto it = exclusivityLeaves_.find(eventStack.Back().EventInstanceId());

        if (it == exclusivityLeaves_.end())
        {
            t.ExclusiveDuration = a.ExclusiveDuration();
            t.ExclusiveCPUTime = a.ExclusiveCPUTime();
        }
        else
        {
            t.ExclusiveDuration = t.Duration;
            t.ExclusiveCPUTime = t.CPUTime;

            exclusivityLeaves_.erase(it);

            return BI::AnalysisControl::CONTINUE;
        }

        size_t stackSize = eventStack.Size();

        if (stackSize <= 1) {
            return BI::AnalysisControl::CONTINUE;
        }

        auto child = eventStack.Back();
        auto parent = eventStack[stackSize - 2];

        unsigned long long childEventId = eventStack.Back().EventId();

        if (childEventId == parent.EventId()) {
            return BI::AnalysisControl::CONTINUE;
        }

        if (    childEventId == BI::EVENT_ID_FUNCTION 
            ||  childEventId == BI::EVENT_ID_FRONT_END_FILE)
        {
            exclusivityLeaves_.insert(eventStack[stackSize - 2].EventInstanceId());
        }

        return BI::AnalysisControl::CONTINUE;
    }

private:
    unsigned pass_;

    std::unordered_map<unsigned long long, TimingData> timingData_;
    std::unordered_set<unsigned long long> exclusivityLeaves_;
    
};

} // namespace vcperf