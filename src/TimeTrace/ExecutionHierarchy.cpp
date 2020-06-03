#include "ExecutionHierarchy.h"

#include <assert.h>
#include <string>

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

using namespace vcperf;

namespace
{
    std::string ToString(const std::wstring& wstring)
    {
        assert(!wstring.empty());

        const UINT codePage = CP_UTF8;
        int requiredSize = WideCharToMultiByte(codePage, 0, wstring.c_str(), static_cast<int>(wstring.size()),
                                               NULL, 0, NULL, NULL);
        std::string convertedString = std::string(requiredSize, '\0');
        WideCharToMultiByte(codePage, 0, wstring.c_str(), static_cast<int>(wstring.size()),
                            &convertedString[0], requiredSize, NULL, NULL);

        return convertedString;
    }

    long long ConvertTickPrecision(long long ticks, long long fromFreq, long long toFreq)
    {
        if (fromFreq <= 0) {
            return 0;
        }

        long long p1 = (ticks / fromFreq) * toFreq;
        long long p2 = (ticks % fromFreq) * toFreq / fromFreq;

        return p1 + p2;
    }

    std::chrono::nanoseconds ConvertTime(long long ticks, long long frequency)
    {
        return std::chrono::nanoseconds{ ConvertTickPrecision(ticks, frequency, std::chrono::nanoseconds::period::den) };
    }

}  // anonymous namespace

bool ExecutionHierarchy::Entry::OverlapsWith(const Entry* other) const
{
    return StartTimestamp < other->StopTimestamp &&
           other->StartTimestamp < StopTimestamp;
}

ExecutionHierarchy::ExecutionHierarchy(const Filter& filter) :
    entries_{},
    roots_{},
    filter_{filter},
    symbolNames_{},
    unresolvedTemplateInstantiationsPerSymbol_{}
{
}

AnalysisControl ExecutionHierarchy::OnStartActivity(const EventStack& eventStack)
{
    if (   MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnNestedActivity)
        || MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnRootActivity))
    {}

    if (   MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnInvocation)
        || MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnFrontEndFile)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnThread))
    {}

    return AnalysisControl::CONTINUE;
}

AnalysisControl ExecutionHierarchy::OnStopActivity(const EventStack& eventStack)
{
    MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnFinishActivity);

    // apply filtering
    if (   MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnFinishFunction)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnFinishNestedTemplateInstantiation)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnFinishRootTemplateInstantiation))
    {}

    return AnalysisControl::CONTINUE;
}

AnalysisControl ExecutionHierarchy::OnSimpleEvent(const EventStack& eventStack)
{
    if (   MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnSymbolName)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnCommandLine)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnEnvironmentVariable)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnFileInput)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnFileOutput))
    {}

    return AnalysisControl::CONTINUE;
}

const ExecutionHierarchy::Entry* ExecutionHierarchy::GetEntry(unsigned long long id) const
{
    auto it = entries_.find(id);
    return it != entries_.end() ? &it->second : nullptr;
}

void ExecutionHierarchy::OnRootActivity(const Activity& root)
{
    Entry* entry = CreateEntry(root);
    
    assert(std::find(roots_.begin(), roots_.end(), entry) == roots_.end());
    roots_.push_back(entry);
}

void ExecutionHierarchy::OnNestedActivity(const Activity& parent, const Activity& child)
{
    auto parentEntryIt = entries_.find(parent.EventInstanceId());
    assert(parentEntryIt != entries_.end());

    Entry* childEntry = CreateEntry(child);

    auto& children = parentEntryIt->second.Children;
    assert(std::find(children.begin(), children.end(), childEntry) == children.end());
    children.push_back(childEntry);
}

void ExecutionHierarchy::OnFinishActivity(const Activity& activity)
{
    auto it = entries_.find(activity.EventInstanceId());
    assert(it != entries_.end());

    it->second.StopTimestamp = ConvertTime(activity.StopTimestamp(), activity.TickFrequency());
}

ExecutionHierarchy::Entry* ExecutionHierarchy::CreateEntry(const Activity& activity)
{
    assert(entries_.find(activity.EventInstanceId()) == entries_.end());

    Entry& entry = entries_[activity.EventInstanceId()];

    entry.Id = activity.EventInstanceId();
    entry.ProcessId = activity.ProcessId();
    entry.ThreadId = activity.ThreadId();
    entry.StartTimestamp = ConvertTime(activity.StartTimestamp(), activity.TickFrequency());
    entry.StopTimestamp = ConvertTime(activity.StopTimestamp(), activity.TickFrequency());
    entry.Name = activity.EventName();

    return &entry;
}

void ExecutionHierarchy::OnInvocation(const Invocation& invocation)
{
    auto it = entries_.find(invocation.EventInstanceId());
    assert(it != entries_.end());

    // may not be present, as it's not available in earlier versions of the toolset
    if (invocation.ToolPath()) {
        it->second.Properties.try_emplace("Tool Path", ToString(invocation.ToolPath()));
    }

    it->second.Properties.try_emplace("Working Directory", ToString(invocation.WorkingDirectory()));
    it->second.Properties.try_emplace("Tool Version", invocation.ToolVersionString());

    if (invocation.EventId() == EVENT_ID_COMPILER) {
        it->second.Name = "CL Invocation " + std::to_string(invocation.InvocationId());
    }
    else if (invocation.EventId() == EVENT_ID_LINKER) {
        it->second.Name = "Link Invocation " + std::to_string(invocation.InvocationId());
    }
}

void ExecutionHierarchy::OnFrontEndFile(const FrontEndFile& frontEndFile)
{
    auto it = entries_.find(frontEndFile.EventInstanceId());
    assert(it != entries_.end());
    it->second.Name = frontEndFile.Path();
}

void ExecutionHierarchy::OnThread(const Activity& parent, const Thread& thread)
{
    auto it = entries_.find(thread.EventInstanceId());
    assert(it != entries_.end());
    it->second.Name = std::string(parent.EventName()) + std::string(thread.EventName());
}

void ExecutionHierarchy::OnFinishFunction(const Activity& parent, const Function& function)
{
    // filter by duration
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(function.Duration());
    if (durationMs < filter_.IgnoreFunctionUnderMs) {
        IgnoreEntry(function.EventInstanceId(), parent.EventInstanceId());
    }
    else
    {
        auto it = entries_.find(function.EventInstanceId());
        assert(it != entries_.end());
        it->second.Name = function.Name();
    }
}

void ExecutionHierarchy::OnFinishRootTemplateInstantiation(const Activity& parent, const TemplateInstantiation& templateInstantiation)
{
    if (!filter_.AnalyzeTemplates) {
        IgnoreEntry(templateInstantiation.EventInstanceId(), parent.EventInstanceId());
    }
    else
    {
        // keep full hierarchy when root TemplateInstantiation passes filter, even if children wouldn't pass
        auto rootDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(templateInstantiation.Duration());
        if (rootDurationMs < filter_.IgnoreTemplateInstantiationUnderMs) {
            IgnoreEntry(templateInstantiation.EventInstanceId(), parent.EventInstanceId());
        }
        else
        {
            // get us subscribed for name resolution (may already have some other activities following)
            auto result = unresolvedTemplateInstantiationsPerSymbol_.try_emplace(templateInstantiation.SpecializationSymbolKey(),
                                                                                 TUnresolvedTemplateInstantiationNames());
            result.first->second.push_back(templateInstantiation.EventInstanceId());
        }
    }
}

void ExecutionHierarchy::OnFinishNestedTemplateInstantiation(const TemplateInstantiationGroup& templateInstantiationGroup,
                                                             const TemplateInstantiation& templateInstantiation)
{
    assert(templateInstantiationGroup.Size() > 0);

    if (!filter_.AnalyzeTemplates) {
        IgnoreEntry(templateInstantiation.EventInstanceId(), templateInstantiationGroup.Back().EventInstanceId());
    }
    else
    {
        // get us subscribed for name resolution (may already have some other activities following)
        auto result = unresolvedTemplateInstantiationsPerSymbol_.try_emplace(templateInstantiation.SpecializationSymbolKey(),
                                                                                TUnresolvedTemplateInstantiationNames());
        result.first->second.push_back(templateInstantiation.EventInstanceId());
    }
}

void ExecutionHierarchy::OnSymbolName(const SymbolName& symbolName)
{
    // SymbolName events get executed after all TemplateInstantiation in the same FrontEndPass take place
    // and we're sure to have exclusive keys for these symbols (they may have matching names between
    // FrontEndPass activities, but their key is unique for the trace)
    assert(symbolNames_.find(symbolName.Key()) == symbolNames_.end());
    std::string& name = symbolNames_[symbolName.Key()];
    name = symbolName.Name();

    // now we've resolved the name, let subscribed activities know
    auto itSubscribedForSymbol = unresolvedTemplateInstantiationsPerSymbol_.find(symbolName.Key());
    if (itSubscribedForSymbol != unresolvedTemplateInstantiationsPerSymbol_.end())
    {
        for (unsigned long long id : itSubscribedForSymbol->second)
        {
            auto itEntry = entries_.find(id);
            
            // may've been filtered out (didn't clean up the subscription as we're cleaning them all in a bit)
            if (itEntry != entries_.end())
            {
                itEntry->second.Name = name;
            }
        }
        itSubscribedForSymbol->second.clear();
    }
}

void ExecutionHierarchy::OnCommandLine(const Activity& parent, const CommandLine& commandLine)
{
    auto it = entries_.find(parent.EventInstanceId());
    assert(it != entries_.end());

    it->second.Properties.try_emplace("Command Line", ToString(commandLine.Value()));
}

void ExecutionHierarchy::OnEnvironmentVariable(const Activity& parent, const EnvironmentVariable& environmentVariable)
{
    // we're not interested in all of them, only the ones that impact the build process
    bool process = false;
    if (parent.EventId() == EVENT_ID::EVENT_ID_COMPILER)
    {
        process = _wcsicmp(environmentVariable.Name(), L"CL") == 0
               || _wcsicmp(environmentVariable.Name(), L"_CL_") == 0
               || _wcsicmp(environmentVariable.Name(), L"INCLUDE") == 0
               || _wcsicmp(environmentVariable.Name(), L"LIBPATH") == 0
               || _wcsicmp(environmentVariable.Name(), L"PATH") == 0;
    }
    else if (parent.EventId() == EVENT_ID::EVENT_ID_LINKER)
    {
        process = _wcsicmp(environmentVariable.Name(), L"LINK") == 0
               || _wcsicmp(environmentVariable.Name(), L"_LINK_") == 0
               || _wcsicmp(environmentVariable.Name(), L"LIB") == 0
               || _wcsicmp(environmentVariable.Name(), L"PATH") == 0
               || _wcsicmp(environmentVariable.Name(), L"TMP") == 0;
    }

    if (process)
    {
        auto it = entries_.find(parent.EventInstanceId());
        assert(it != entries_.end());
        
        it->second.Properties.try_emplace("Env Var: " + ToString(environmentVariable.Name()), ToString(environmentVariable.Value()));
    }
}

void ExecutionHierarchy::OnFileInput(const Activity& parent, const FileInput& fileInput)
{
    auto it = entries_.find(parent.EventInstanceId());
    assert(it != entries_.end());

    it->second.Properties.try_emplace("File Input", ToString(fileInput.Path()));
}

void ExecutionHierarchy::OnFileOutput(const Activity& parent, const FileOutput& fileOutput)
{
    auto it = entries_.find(parent.EventInstanceId());
    assert(it != entries_.end());

    it->second.Properties.try_emplace("File Output", ToString(fileOutput.Path()));
}

void ExecutionHierarchy::IgnoreEntry(unsigned long long id, unsigned long long parentId)
{
    // ensure parent no longer points to it
    auto itParent = entries_.find(parentId);
    assert(itParent != entries_.end());

    std::vector<Entry*>& children = itParent->second.Children;
    auto itEntryAsChildren = std::find_if(children.begin(), children.end(), [&id](const Entry* child) {
        return child->Id == id;
    });
    assert(itEntryAsChildren != children.end());

    children.erase(itEntryAsChildren);

    // ignore it and its children (no need to let intermediate parents know, as they'll be erased as well)
    IgnoreEntry(id);
}

void ExecutionHierarchy::IgnoreEntry(unsigned long long id)
{
    auto itEntry = entries_.find(id);
    if (itEntry != entries_.end())
    {
        for (const Entry* child : itEntry->second.Children) {
            IgnoreEntry(child->Id);
        }

        entries_.erase(itEntry);
    }
}
