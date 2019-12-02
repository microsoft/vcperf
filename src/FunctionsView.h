#pragma once

#include "CppBuildInsights.hpp"
#include "ContextBuilder.h"
#include "TimingDataCache.h"

using namespace Microsoft::Cpp::BuildInsights;

using namespace Activities;
using namespace SimpleEvents;

class FunctionsView : public IRelogger
{
public:
    FunctionsView(ContextBuilder* contextBuilder,
        TimingDataCache* timingDataCache):
        contextBuilder_{contextBuilder},
        timingDataCache_{timingDataCache}
    {}

    AnalysisControl OnStartActivity(const EventStack& eventStack, 
        const void* relogSession) override
    {
        return OnActivity(eventStack, relogSession);
    }

    AnalysisControl OnSimpleEvent(const EventStack& eventStack,
        const void* relogSession) override;

private:
    AnalysisControl OnActivity(const EventStack& eventStack, const void* relogSession);

    void EmitFunctionActivity(Function func, const void* relogSession);

    void EmitFunctionForceInlinee(Function func, ForceInlinee forceInlinee,
        const void* relogSession);

    ContextBuilder* contextBuilder_;
    TimingDataCache* timingDataCache_;

};