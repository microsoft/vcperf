#pragma once

#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <assert.h>

#include "CppBuildInsights.hpp"

using namespace Microsoft::Cpp::BuildInsights;

using namespace Activities;

class MiscellaneousCache : public IAnalyzer
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
    };
    
    const TimingData& GetTimingData(const Activity& a)
    {
        return timingData_[a.InstanceId()];
    }

    AnalysisControl OnEndAnalysisPass() override
    {
        ++pass_;

        return AnalysisControl::CONTINUE;
    }

    AnalysisControl OnStopActivity(const EventStack& eventStack) override
    {
        if (pass_) {
            return AnalysisControl::CONTINUE;
        }

        Activity a{eventStack.Back()};

        assert(timingData_.find(a.InstanceId()) == timingData_.end());

        auto& t = timingData_[a.InstanceId()];

        t.Duration = a.Duration();
        t.CPUTime = a.CPUTime();

        auto it = exclusivityLeaves_.find(eventStack.Back().InstanceId());

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

            return AnalysisControl::CONTINUE;
        }

        size_t stackSize = eventStack.Size();

        if (stackSize <= 1) {
            return AnalysisControl::CONTINUE;
        }

        auto& child = eventStack.Back();
        auto& parent = eventStack[stackSize - 2];

        unsigned long long childEntityId = eventStack.Back().EntityId();

        if (childEntityId == parent.EntityId()) {
            return AnalysisControl::CONTINUE;
        }

        if (    childEntityId == Info<Function>::ID
            ||  childEntityId == Info<File>::ID)
        {
            exclusivityLeaves_.insert(eventStack[stackSize - 2].InstanceId());
        }

        return AnalysisControl::CONTINUE;
    }

private:
    unsigned pass_;

    std::unordered_map<unsigned long long, TimingData> timingData_;
    std::unordered_set<unsigned long long> exclusivityLeaves_;
    
};