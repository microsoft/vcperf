#pragma once

#include <filesystem>
#include <iosfwd>
#include <unordered_set>

#include "VcperfBuildInsights.h"
#include "Analyzers\ExecutionHierarchy.h"
#include "Views\ChromeFlameGraph\PackedProcessThreadRemapping.h"

namespace vcperf
{

class ChromeFlameGraphView : public BI::IAnalyzer
{
public:

    // controls which subhierarchies get ignored (check ChromeFlameGraphView::ShouldIgnore)
    struct Filter
    {
        bool AnalyzeTemplates = false;
        std::chrono::milliseconds IgnoreTemplateInstantiationUnderMs = std::chrono::milliseconds(0);
        std::chrono::milliseconds IgnoreFunctionUnderMs = std::chrono::milliseconds(0);
    };

public:

    ChromeFlameGraphView(ExecutionHierarchy* hierarchy, const std::filesystem::path& outputFile,
                         Filter filter);

    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnEndAnalysis() override;

private:

    void ProcessActivity(const A::Activity& activity);
    void CalculateChildrenOffsets(const A::Activity& activity);
    void ExportTo(std::ostream& outputStream) const;
    bool ShouldIgnore(const A::Activity& activity) const;

    ExecutionHierarchy* hierarchy_;
    std::filesystem::path outputFile_;
    PackedProcessThreadRemapping remappings_;
    std::unordered_set<unsigned long long> ignoredEntries_;
    Filter filter_;
};

} // namespace vcperf
