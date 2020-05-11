#include "ChromeFlameGraphView.h"

#include <fstream>
#include <nlohmann\json.hpp>

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace vcperf;

// anonymous namespace to hold auxiliary functions (instead of using it in the class and requiring #include <nlohmann\json.hpp>
namespace
{

void AddEntry(const ExecutionHierarchy::Entry* entry, nlohmann::json& traceEvents,
                const PackedProcessThreadRemapping& remappings, const std::unordered_set<unsigned long long>& ignoredEntries)
{
    if (ignoredEntries.find(entry->Id) != ignoredEntries.end())
    {
        return;
    }

    const PackedProcessThreadRemapping::Remap* remap = remappings.GetRemapFor(entry->Id);
    unsigned long processId = remap != nullptr ? remap->ProcessId : entry->ProcessId;
    unsigned long threadId = remap != nullptr ? remap->ThreadId : entry->ThreadId;

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

        // add extra properties, if any
        if (!entry->Properties.empty())
        {
            nlohmann::json args = nlohmann::json::object();
            for (auto pair : entry->Properties)
            {
                args[pair.first] = pair.second;
            }

            completeEvent["args"] = args;
        }

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

        // add extra properties, if any
        if (!entry->Properties.empty())
        {
            nlohmann::json args = nlohmann::json::object();
            for (auto pair : entry->Properties)
            {
                args[pair.first] = pair.second;
            }

            beginEvent["args"] = args;
        }

        traceEvents.push_back(beginEvent);

        for (const ExecutionHierarchy::Entry* child : entry->Children)
        {
            AddEntry(child, traceEvents, remappings, ignoredEntries);
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

}  // anonymous namespace

ChromeFlameGraphView::ChromeFlameGraphView(ExecutionHierarchy* hierarchy, const std::filesystem::path& outputFile,
                                           Filter filter) :
    hierarchy_{hierarchy},
    outputFile_{outputFile},
    remappings_{},
    ignoredEntries_{},
    filter_{filter}
{
}

BI::AnalysisControl ChromeFlameGraphView::OnStopActivity(const BI::EventStack& eventStack)
{
    if (MatchEventInMemberFunction(eventStack.Back(), this, &ChromeFlameGraphView::ProcessActivity))
    {}

    return AnalysisControl::CONTINUE;
}

AnalysisControl ChromeFlameGraphView::OnEndAnalysis()
{
    remappings_.Calculate(hierarchy_);

    std::ofstream outputStream(outputFile_);
    if (!outputStream)
    {
        return AnalysisControl::FAILURE;
    }

    ExportTo(outputStream);
    outputStream.close();

    return AnalysisControl::CONTINUE;
}

void ChromeFlameGraphView::ProcessActivity(const Activity& activity)
{    
    if (ShouldIgnore(activity))
    {
        ignoredEntries_.emplace(activity.EventInstanceId());
    }
    else
    {
        CalculateChildrenOffsets(activity);
    }
}

void ChromeFlameGraphView::CalculateChildrenOffsets(const Activity& activity)
{
    const ExecutionHierarchy::Entry* entry = hierarchy_->GetEntry(activity.EventInstanceId());
    assert(entry != nullptr);

    remappings_.CalculateChildrenLocalThreadData(entry);
}

void ChromeFlameGraphView::ExportTo(std::ostream& outputStream) const
{
    nlohmann::json json = nlohmann::json::object();

    // add hierarchy
    nlohmann::json traceEvents = nlohmann::json::array();
    for (const ExecutionHierarchy::Entry* root : hierarchy_->GetRoots())
    {
        AddEntry(root, traceEvents, remappings_, ignoredEntries_);
    }
    json["traceEvents"] = traceEvents;

    // although "ms" is the default time unit, make it explicit ("ms" means "microseconds")
    json["displayTimeUnit"] = "ms";

    outputStream << std::setw(2) << json << std::endl;
}

bool ChromeFlameGraphView::ShouldIgnore(const A::Activity& activity) const
{
    if (activity.EventId() == EVENT_ID_TEMPLATE_INSTANTIATION)
    {
        if (!filter_.AnalyzeTemplates)
        {
            return true;
        }

        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(activity.Duration());
        if (durationMs < filter_.IgnoreTemplateInstantiationUnderMs)
        {
            return true;
        }
    }

    if (activity.EventId() == EVENT_ID_FUNCTION)
    {
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(activity.Duration());
        if (durationMs < filter_.IgnoreFunctionUnderMs)
        {
            return true;
        }
    }

    return false;
}
