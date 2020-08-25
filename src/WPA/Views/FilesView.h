#pragma once

#include "VcperfBuildInsights.h"
#include "WPA\Analyzers\ContextBuilder.h"
#include "WPA\Analyzers\MiscellaneousCache.h"

namespace vcperf
{

class FilesView : public BI::IRelogger
{
public:
    FilesView(ContextBuilder* contextBuilder,
        MiscellaneousCache* miscellaneousCache) :
        contextBuilder_{contextBuilder},
        miscellaneousCache_{miscellaneousCache}
    {}

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack,
        const void* relogSession) override;

    void OnFileParse(const A::FrontEndFileGroup& files, const void* relogSession);

private:
    ContextBuilder* contextBuilder_;
    MiscellaneousCache* miscellaneousCache_;
};

} // namespace vcperf