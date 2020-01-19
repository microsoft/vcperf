#pragma once

#include "CppBuildInsights.hpp"
#include "Utility.h"
#include "PayloadBuilder.h"
#include "ContextBuilder.h"
#include "ExpensiveTemplateInstantiationCache.h"
#include "MiscellaneousCache.h"

using namespace Microsoft::Cpp::BuildInsights;

class TemplateInstantiationsView : public IRelogger
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

    AnalysisControl OnStartActivity(const EventStack& eventStack, const void* relogSession) override
    {
        if (!isEnabled_) {
            return AnalysisControl::CONTINUE;
        }

        MatchEventStackInMemberFunction(eventStack, this, 
            &TemplateInstantiationsView::OnTemplateInstantiationStart, relogSession);

        return AnalysisControl::CONTINUE;
    }

    void OnTemplateInstantiationStart(TemplateInstantiation ti, const void* relogSession);

private:
    ContextBuilder* contextBuilder_;
    const ExpensiveTemplateInstantiationCache* tiCache_;
    MiscellaneousCache* miscellaneousCache_;

    bool isEnabled_;
};