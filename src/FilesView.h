#pragma once

#include "CppBuildInsights.hpp"
#include "ContextBuilder.h"
#include "TimingDataCache.h"

using namespace Microsoft::Cpp::BuildInsights;

class FilesView : public IRelogger
{
public:
    FilesView(ContextBuilder* contextBuilder,
        TimingDataCache* timingDataCache) :
        contextBuilder_{contextBuilder},
        timingDataCache_{timingDataCache}
    {}

    AnalysisControl OnStartActivity(const EventStack& eventStack,
        const void* relogSession) override;

    void OnFileParse(FileGroup files, const void* relogSession);

private:
    ContextBuilder* contextBuilder_;
    TimingDataCache* timingDataCache_;
};