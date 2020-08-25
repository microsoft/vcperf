#pragma once

#include "VcperfBuildInsights.h"
#include "Utility.h"
#include "PayloadBuilder.h"
#include "WPA\Analyzers\ContextBuilder.h"
#include "WPA\Analyzers\ExpensiveTemplateInstantiationCache.h"
#include "WPA\Analyzers\MiscellaneousCache.h"

namespace vcperf
{

class TemplateInstantiationsView : public BI::IRelogger
{
public:
    TemplateInstantiationsView(
        ContextBuilder* contextBuilder,
        const ExpensiveTemplateInstantiationCache* tiCache,
        MiscellaneousCache* miscellaneousCache,
        bool isEnabled) :
        contextBuilder_{contextBuilder},
        tiCache_{tiCache},
        miscellaneousCache_{miscellaneousCache},
        isEnabled_{isEnabled}
    {}

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack, const void* relogSession) override
    {
        if (!isEnabled_) {
            return BI::AnalysisControl::CONTINUE;
        }

        MatchEventStackInMemberFunction(eventStack, this, 
            &TemplateInstantiationsView::OnTemplateInstantiationStart, relogSession);

        return BI::AnalysisControl::CONTINUE;
    }

    void OnTemplateInstantiationStart(const A::TemplateInstantiation& ti, const void* relogSession);

private:
    ContextBuilder* contextBuilder_;
    const ExpensiveTemplateInstantiationCache* tiCache_;
    MiscellaneousCache* miscellaneousCache_;

    bool isEnabled_;
};

} // namespace vcperf