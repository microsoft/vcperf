#include "TimeTraceGenerator.h"

#include <fstream>
#include <nlohmann\json.hpp>

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace vcperf;

// anonymous namespace to hold auxiliary functions (instead of using it in the class and requiring #include <nlohmann\json.hpp>
namespace
{

void AddEntry(const ExecutionHierarchy::Entry* entry, nlohmann::json& traceEvents, const PackedProcessThreadRemapping& remappings)
{
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
            for (auto& pair : entry->Properties)
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
            for (auto& pair : entry->Properties)
            {
                args[pair.first] = pair.second;
            }

            beginEvent["args"] = args;
        }

        traceEvents.push_back(beginEvent);

        for (const ExecutionHierarchy::Entry* child : entry->Children)
        {
            AddEntry(child, traceEvents, remappings);
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

TimeTraceGenerator::TimeTraceGenerator(ExecutionHierarchy* hierarchy, const std::filesystem::path& outputFile) :
    hierarchy_{hierarchy},
    outputFile_{outputFile},
    remappings_{}
{
}

BI::AnalysisControl TimeTraceGenerator::OnStopActivity(const BI::EventStack& eventStack)
{
    MatchEventInMemberFunction(eventStack.Back(), this, &TimeTraceGenerator::ProcessActivity);

    return AnalysisControl::CONTINUE;
}

AnalysisControl TimeTraceGenerator::OnEndAnalysis()
{
    remappings_.Calculate(hierarchy_);

    std::ofstream outputStream(outputFile_);
    if (!outputStream) {
        return AnalysisControl::FAILURE;
    }

    ExportTo(outputStream);
    outputStream.close();

    return AnalysisControl::CONTINUE;
}

void TimeTraceGenerator::ProcessActivity(const Activity& activity)
{
    CalculateChildrenOffsets(activity);
}

void TimeTraceGenerator::CalculateChildrenOffsets(const Activity& activity)
{
    const ExecutionHierarchy::Entry* entry = hierarchy_->GetEntry(activity.EventInstanceId());
    
    // may've been filtered out!
    if (entry != nullptr)
    {
        remappings_.CalculateChildrenLocalThreadData(entry);
    }
}

void TimeTraceGenerator::ExportTo(std::ostream& outputStream) const
{
    nlohmann::json json = nlohmann::json::object();

    // add hierarchy
    nlohmann::json traceEvents = nlohmann::json::array();
    for (const ExecutionHierarchy::Entry* root : hierarchy_->GetRoots())
    {
        AddEntry(root, traceEvents, remappings_);
    }
    json["traceEvents"] = traceEvents;

    // although "ms" is the default time unit, make it explicit ("ms" means "microseconds")
    json["displayTimeUnit"] = "ms";

    outputStream << std::setw(2) << json << std::endl;
}
