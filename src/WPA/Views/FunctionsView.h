#pragma once

#include "VcperfBuildInsights.h"
#include "WPA\Analyzers\ContextBuilder.h"
#include "WPA\Analyzers\MiscellaneousCache.h"

namespace vcperf
{

class FunctionsView : public BI::IRelogger
{
public:
    FunctionsView(ContextBuilder* contextBuilder,
        MiscellaneousCache* miscellaneousCache):
        contextBuilder_{contextBuilder},
        miscellaneousCache_{miscellaneousCache}
    {}

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack, 
        const void* relogSession) override
    {
        return OnActivity(eventStack, relogSession);
    }

    BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack,
        const void* relogSession) override;

private:
    BI::AnalysisControl OnActivity(const BI::EventStack& eventStack, const void* relogSession);

    void EmitFunctionActivity(A::Function func, const void* relogSession);

    void EmitFunctionForceInlinee(const A::Function& func, const SE::ForceInlinee& forceInlinee,
        const void* relogSession);

    ContextBuilder* contextBuilder_;
    MiscellaneousCache* miscellaneousCache_;

};

} // namespace vcperf