#include "ChromeFlameGraphView.h"

#include <fstream>

using namespace Microsoft::Cpp::BuildInsights;
using namespace vcperf;

ChromeFlameGraphView::ChromeFlameGraphView(ExecutionHierarchy* hierarchy, const std::filesystem::path& outputFile,
                                           bool analyzeTemplates) :
    hierarchy_{hierarchy},
    outputFile_{outputFile},
    analyzeTemplates_{analyzeTemplates},
    remappings_{}
{
}

AnalysisControl ChromeFlameGraphView::OnEndAnalysis()
{
    CalculatePackedProcessThreadRemapping();

    std::ofstream outputStream(outputFile_);
    if (!outputStream)
    {
        return AnalysisControl::FAILURE;
    }

    ExportTo(outputStream);
    outputStream.close();

    return AnalysisControl::CONTINUE;
}

void ChromeFlameGraphView::CalculatePackedProcessThreadRemapping()
{
}

void ChromeFlameGraphView::ExportTo(std::ostream& outputStream) const
{
    nlohmann::json json = nlohmann::json::object();

    // add hierarchy
    nlohmann::json traceEvents = nlohmann::json::array();
    for (const ExecutionHierarchy::Entry* root : hierarchy_->GetRoots())
    {
        AddEntry(root, traceEvents);
    }
    json["traceEvents"] = traceEvents;

    // although "ms" is the default time unit, make it explicit ("ms" means "microseconds")
    json["displayTimeUnit"] = "ms";

    outputStream << std::setw(2) << json << std::endl;
}

void ChromeFlameGraphView::AddEntry(const ExecutionHierarchy::Entry* entry, nlohmann::json& traceEvents) const
{
    const PackedProcessThreadRemapping::Remap* remap = remappings_.GetRemapFor(entry->Id);
    unsigned long processId = remap != nullptr ? remap->ProcessId : entry->ProcessId;
    unsigned long threadId  = remap != nullptr ? remap->ThreadId  : entry->ThreadId;

    if (entry->Children.size() == 0)
    {
        auto startTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(entry->StartTimestamp);
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(entry->StopTimestamp - entry->StartTimestamp);
        nlohmann::json completeEvent =
        {
            { "ph", "X" },
            { "pid", processId },
            { "tid", threadId },
            { "name", entry->Name },
            { "ts", startTimestamp.count() },
            { "dur", duration.count() }
        };

        traceEvents.push_back(completeEvent);
    }
    else
    {
        auto startTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(entry->StartTimestamp);
        nlohmann::json beginEvent =
        {
            { "ph", "B" },
            { "pid", processId },
            { "tid", threadId },
            { "name", entry->Name },
            { "ts", startTimestamp.count() }
        };
        traceEvents.push_back(beginEvent);

        for (const ExecutionHierarchy::Entry* child : entry->Children)
        {
            AddEntry(child, traceEvents);
        }

        auto stopTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(entry->StopTimestamp);
        nlohmann::json endEvent =
        {
            { "ph", "E" },
            { "pid", processId },
            { "tid", threadId },
            { "ts", stopTimestamp.count() }
        };
        traceEvents.push_back(endEvent);
    }
}
