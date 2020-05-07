#pragma once

#include <filesystem>
#include <nlohmann\json.hpp>

#include "VcperfBuildInsights.h"
#include "Analyzers\ExecutionHierarchy.h"

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

    ExecutionHierarchy* hierarchy_;
    std::filesystem::path outputFile_;
    bool analyzeTemplates_;
};

} // namespace vcperf
