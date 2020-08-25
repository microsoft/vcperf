#pragma once

#include <filesystem>
#include <iosfwd>
#include <unordered_set>

#include "VcperfBuildInsights.h"
#include "TimeTrace\ExecutionHierarchy.h"
#include "TimeTrace\PackedProcessThreadRemapping.h"

namespace vcperf
{

class TimeTraceGenerator : public BI::IAnalyzer
{
public:

    TimeTraceGenerator(ExecutionHierarchy* hierarchy, const std::filesystem::path& outputFile);

    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnEndAnalysis() override;

private:

    void ProcessActivity(const A::Activity& activity);

    void CalculateChildrenOffsets(const A::Activity& activity);
    void ExportTo(std::ostream& outputStream) const;

    ExecutionHierarchy* hierarchy_;
    std::filesystem::path outputFile_;
    PackedProcessThreadRemapping remappings_;
};

} // namespace vcperf
