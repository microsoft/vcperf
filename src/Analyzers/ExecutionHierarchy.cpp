#include "ExecutionHierarchy.h"

#include <assert.h>

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

using namespace vcperf;

namespace
{
    std::string ToString(const std::wstring& wstring)
    {
        assert(!wstring.empty());

        const UINT codePage = CP_ACP;
        int requiredSize = WideCharToMultiByte(codePage, 0, wstring.c_str(), static_cast<int>(wstring.size()),
                                               NULL, 0, NULL, NULL);
        std::string convertedString = std::string(requiredSize, '\0');
        WideCharToMultiByte(codePage, 0, wstring.c_str(), static_cast<int>(wstring.size()),
                            &convertedString[0], requiredSize, NULL, NULL);

        return convertedString;
    }

    std::chrono::nanoseconds ConvertTime(long long ticks, long long frequency)
    {
        return std::chrono::nanoseconds{ Internal::ConvertTickPrecision(ticks, frequency, std::chrono::nanoseconds::period::den) };
    }

}  // anonymous namespace

bool ExecutionHierarchy::Entry::OverlapsWith(const Entry* other) const
{
    return StartTimestamp < other->StopTimestamp &&
           other->StartTimestamp < StopTimestamp;
}

ExecutionHierarchy::ExecutionHierarchy() :
    entries_{},
    roots_{},
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
        || MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnFunction)
        || MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnTemplateInstantiation))
    {}

    return AnalysisControl::CONTINUE;
}

AnalysisControl ExecutionHierarchy::OnStopActivity(const EventStack& eventStack)
{
    if (MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnFinishActivity))
    {}

    return AnalysisControl::CONTINUE;
}

AnalysisControl ExecutionHierarchy::OnSimpleEvent(const EventStack& eventStack)
{
    if (   MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnSymbolName)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnCommandLine)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnEnvironmentVariable))
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
    if (invocation.ToolPath() != nullptr)
    {
        it->second.Properties.try_emplace("Tool Path", ToString(invocation.ToolPath()));
    }

    it->second.Properties.try_emplace("Working Directory", ToString(invocation.WorkingDirectory()));
    it->second.Properties.try_emplace("Tool Version", invocation.ToolVersionString());
}

void ExecutionHierarchy::OnFrontEndFile(const FrontEndFile& frontEndFile)
{
    auto it = entries_.find(frontEndFile.EventInstanceId());
    assert(it != entries_.end());
    it->second.Name = frontEndFile.Path();
}

void ExecutionHierarchy::OnFunction(const Function& function)
{
    auto it = entries_.find(function.EventInstanceId());
    assert(it != entries_.end());
    it->second.Name = function.Name();
}

void ExecutionHierarchy::OnTemplateInstantiation(const TemplateInstantiation& templateInstantiation)
{
    // SymbolName events get executed after all TemplateInstantiation in the same FrontEndPass take place
    // and we're sure to have exclusive keys for these symbols (they may have matching names between
    // FrontEndPass activities, but their key is unique for the trace)
    assert(symbolNames_.find(templateInstantiation.SpecializationSymbolKey()) == symbolNames_.end());
    auto itSymbol = symbolNames_.find(templateInstantiation.SpecializationSymbolKey());

    // get us subscribed for name resolution (may already have some other activities following)
    auto result = unresolvedTemplateInstantiationsPerSymbol_.try_emplace(templateInstantiation.SpecializationSymbolKey(),
                                                                            TUnresolvedTemplateInstantiationNames());
    result.first->second.push_back(templateInstantiation.EventInstanceId());
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
            assert(itEntry != entries_.end());
            itEntry->second.Name = name;
        }
        itSubscribedForSymbol->second.clear();
    }
}

void ExecutionHierarchy::OnCommandLine(const A::Activity& parent, const CommandLine& commandLine)
{
    auto it = entries_.find(parent.EventInstanceId());
    assert(it != entries_.end());

    it->second.Properties.try_emplace("Command Line", ToString(commandLine.Value()));
}

void ExecutionHierarchy::OnEnvironmentVariable(const A::Activity& parent, const EnvironmentVariable& environmentVariable)
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
