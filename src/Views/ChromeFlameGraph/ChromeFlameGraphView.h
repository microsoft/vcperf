#pragma once

#include <filesystem>
#include <iosfwd>
#include <nlohmann\json.hpp>

#include "VcperfBuildInsights.h"
#include "Analyzers\ExecutionHierarchy.h"
#include "Views\ChromeFlameGraph\PackedProcessThreadRemapping.h"

namespace vcperf
{

class ChromeFlameGraphView : public BI::IAnalyzer
{
public:

    ChromeFlameGraphView(ExecutionHierarchy* hierarchy, const std::filesystem::path& outputFile,
                         bool analyzeTemplates);

    BI::AnalysisControl OnEndAnalysis() override;

private:

    void AddEntry(const ExecutionHierarchy::Entry* entry, nlohmann::json& traceEvents) const;
    void ExportTo(std::ostream& outputStream) const;

    ExecutionHierarchy* hierarchy_;
    std::filesystem::path outputFile_;
    bool analyzeTemplates_;
    PackedProcessThreadRemapping remappings_;
};

} // namespace vcperf
