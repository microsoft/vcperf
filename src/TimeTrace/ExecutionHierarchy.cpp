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

    unsigned int CountDigits(size_t number)
    {
        unsigned int digits;
        for (digits = 0; number > 0; ++digits)
        {
            number /= 10;
        }

        return digits;
    }

    std::string PrePadNumber(size_t number, char paddingCharacter, size_t totalExpectedLength)
    {
        std::string asPaddedString = std::to_string(number);

        if (asPaddedString.size() < totalExpectedLength) {
            asPaddedString.insert(0, totalExpectedLength - asPaddedString.size(), paddingCharacter);
        }

        return asPaddedString;
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
    fileInputsOutputsPerInvocation_{},
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
    if (   MatchEventInMemberFunction(eventStack.Back(), this, &ExecutionHierarchy::OnFinishInvocation)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnFinishFunction)
        || MatchEventStackInMemberFunction(eventStack, this, &ExecutionHierarchy::OnFinishTemplateInstantiation))
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

void ExecutionHierarchy::OnFinishInvocation(const Invocation& invocation)
{
    // store every FileInput and FileOutput as properties
    auto itFileInputsOutputs = fileInputsOutputsPerInvocation_.find(invocation.EventInstanceId());
    if (itFileInputsOutputs != fileInputsOutputsPerInvocation_.end())
    {
        auto itInvocation = entries_.find(invocation.EventInstanceId());
        assert(itInvocation != entries_.end());

        Entry& invocationEntry = itInvocation->second;
        const TFileInputsOutputs& data = itFileInputsOutputs->second;

        // FileInputs
        if (data.first.size() == 1) {
            invocationEntry.Properties.try_emplace("File Input", data.first[0]);
        }
        else
        {
            const size_t totalDigits = CountDigits(data.first.size());
            for (size_t i = 0; i < data.first.size(); ++i) {
                invocationEntry.Properties.try_emplace("File Input #" + PrePadNumber(i, '0', totalDigits), data.first[i]);
            }
        }

        // FileOutputs
        if (data.second.size() == 1) {
            invocationEntry.Properties.try_emplace("File Output", data.second[0]);
        }
        else
        {
            const size_t totalDigits = CountDigits(data.second.size());
            for (size_t i = 0; i < data.second.size(); ++i) {
                invocationEntry.Properties.try_emplace("File Output #" + PrePadNumber(i, '0', totalDigits), data.second[i]);
            }
        }

        fileInputsOutputsPerInvocation_.erase(invocation.EventInstanceId());
    }
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

void ExecutionHierarchy::OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup)
{
    const A::Activity& parentActivity = templateInstantiationGroup.Size() == 1 ? parent : templateInstantiationGroup[templateInstantiationGroup.Size() - 2];

    if (!filter_.AnalyzeTemplates) {
        IgnoreEntry(templateInstantiationGroup.Back().EventInstanceId(), parentActivity.EventInstanceId());
    }
    else
    {
        // keep full hierarchy when root passes filter, even if children wouldn't pass
        // we can only know root's Duration when it finishes: checking it when a child finishes will result in 0ns
        if (templateInstantiationGroup.Size() == 1 &&
            std::chrono::duration_cast<std::chrono::milliseconds>(templateInstantiationGroup.Front().Duration()) < filter_.IgnoreTemplateInstantiationUnderMs)
        {
            // ignores root TemplateInstantiation and its children (don't clear their symbol subscriptions, we'll deal with missing subscribers in OnSymbolName)
            IgnoreEntry(templateInstantiationGroup.Back().EventInstanceId(), parentActivity.EventInstanceId());
        }
        else
        {
            // get us subscribed for name resolution (may already have some other activities following)
            auto result = unresolvedTemplateInstantiationsPerSymbol_.try_emplace(templateInstantiationGroup.Back().SpecializationSymbolKey(),
                                                                                 TUnresolvedTemplateInstantiationNames());
            result.first->second.push_back(templateInstantiationGroup.Back().EventInstanceId());
        }
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

            // may've been filtered out (didn't clean up this subscription when filtering happened, as we're cleaning them all in a bit)
            if (itEntry != entries_.end()) {
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

void ExecutionHierarchy::OnFileInput(const Invocation& parent, const FileInput& fileInput)
{
    // an Invocation can have several FileInputs, keep track of them and add as properties later on
    auto result = fileInputsOutputsPerInvocation_.try_emplace(parent.EventInstanceId(), TFileInputs(), TFileOutputs());
    auto &inputsOutputsPair = result.first->second;

    std::wstring path = fileInput.Path();

    // A rare bug in the linker causes it to emit FileInput events
    // with an empty path. Ignore them.
    if (path.empty()) {
        return;
    }

    inputsOutputsPair.first.push_back(ToString(path));
}

void ExecutionHierarchy::OnFileOutput(const Invocation& parent, const FileOutput& fileOutput)
{
    // an Invocation can have several FileOutputs, keep track of them and add as properties later on
    auto result = fileInputsOutputsPerInvocation_.try_emplace(parent.EventInstanceId(), TFileInputs(), TFileOutputs());
    auto& inputsOutputsPair = result.first->second;

    inputsOutputsPair.second.push_back(ToString(fileOutput.Path()));
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
