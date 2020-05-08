#include "ExecutionHierarchy.h"

#include <assert.h>

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

using namespace vcperf;

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

    if (   MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnFrontEndFile)
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
    if (MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnSymbolName))
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
    auto itSymbol = symbolNames_.find(templateInstantiation.SpecializationSymbolKey());
    
    // do we have the name already?
    if (itSymbol != symbolNames_.end())
    {
        auto it = entries_.find(templateInstantiation.EventInstanceId());
        assert(it != entries_.end());
        it->second.Name = itSymbol->second;
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

std::chrono::nanoseconds ExecutionHierarchy::ConvertTime(long long ticks, long long frequency) const
{
    return std::chrono::nanoseconds{ Internal::ConvertTickPrecision(ticks, frequency, std::chrono::nanoseconds::period::den) };
}
