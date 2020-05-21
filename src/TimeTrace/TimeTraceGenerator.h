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

    // controls which subhierarchies get ignored (check TimeTraceGenerator::ShouldIgnore)
    struct Filter
    {
        bool AnalyzeTemplates = false;
        std::chrono::milliseconds IgnoreTemplateInstantiationUnderMs = std::chrono::milliseconds(0);
        std::chrono::milliseconds IgnoreFunctionUnderMs = std::chrono::milliseconds(0);
    };

public:

    TimeTraceGenerator(ExecutionHierarchy* hierarchy, const std::filesystem::path& outputFile,
                         Filter filter);

    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnEndAnalysis() override;

private:

    void ProcessActivity(const A::Activity& activity);
    void ProcessFunction(const A::Function& function);
    void ProcessTemplateInstantiationGroup(const A::TemplateInstantiationGroup& templateInstantiationGroup);

    void CalculateChildrenOffsets(const A::Activity& activity);
    void ExportTo(std::ostream& outputStream) const;

    ExecutionHierarchy* hierarchy_;
    std::filesystem::path outputFile_;
    PackedProcessThreadRemapping remappings_;
    std::unordered_set<unsigned long long> ignoredEntries_;
    Filter filter_;
};

} // namespace vcperf
