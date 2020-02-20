#pragma once

#include "CppBuildInsights.hpp"
#include "ContextBuilder.h"
#include "MiscellaneousCache.h"

using namespace Microsoft::Cpp::BuildInsights;

class FilesView : public IRelogger
{
public:
    FilesView(ContextBuilder* contextBuilder,
        MiscellaneousCache* miscellaneousCache) :
        contextBuilder_{contextBuilder},
        miscellaneousCache_{miscellaneousCache}
    {}

    AnalysisControl OnStartActivity(const EventStack& eventStack,
        const void* relogSession) override;

    void OnFileParse(const FrontEndFileGroup& files, const void* relogSession);

private:
    ContextBuilder* contextBuilder_;
    MiscellaneousCache* miscellaneousCache_;
};