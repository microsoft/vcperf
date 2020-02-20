#include "ContextBuilder.h"


AnalysisControl ContextBuilder::OnStartActivity(const EventStack& eventStack)
{
    if (!MustBuildContext()) {
        return AnalysisControl::CONTINUE;
    }

    currentContextData_ = nullptr;
    currentInstanceId_ = 0;

    if (    MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnCompilerPass)
        ||  MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnC2Thread))
    {
        return AnalysisControl::CONTINUE;
    }

    if (    MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnNestedActivity)
        ||  MatchEventInMemberFunction(eventStack.Back(), this, &ContextBuilder::OnRootActivity))
    {
    }

    MatchEventInMemberFunction(eventStack.Back(), this, &ContextBuilder::OnInvocation);
        
    return AnalysisControl::CONTINUE;
}

AnalysisControl ContextBuilder::OnStopActivity(const EventStack& eventStack)
{
    if (!MustBuildContext()) {
        return AnalysisControl::CONTINUE;
    }

    currentContextData_ = nullptr;
    currentInstanceId_ = 0;

    if (    MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnStopNestedActivity)
        ||  MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnStopRootActivity)) 
    {}

    if (    MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnStopCompilerPass)
        ||  MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnStopC2Thread)
        ||  MatchEventStackInMemberFunction(eventStack, this, &ContextBuilder::OnStopInvocation)) 
    {}

    return AnalysisControl::CONTINUE;
}

AnalysisControl ContextBuilder::OnSimpleEvent(const EventStack& eventStack)
{
    if (MustCacheMainComponents()) 
    {
        if (	MatchEventStackInMemberFunction(eventStack, this, 
				    &ContextBuilder::OnLibOutput)

		    ||	MatchEventStackInMemberFunction(eventStack, this, 
				    &ContextBuilder::OnExecutableImageOutput)

            ||	MatchEventStackInMemberFunction(eventStack, this, 
				    &ContextBuilder::OnImpLibOutput)
            
            ||	MatchEventStackInMemberFunction(eventStack, this, 
				    &ContextBuilder::OnCompilerInput)

            ||	MatchEventStackInMemberFunction(eventStack, this, 
				    &ContextBuilder::OnCompilerOutput))
        {}

        return AnalysisControl::CONTINUE;
    }

    if (!MustBuildContext()) {
        return AnalysisControl::CONTINUE;
    }

    currentContextData_ = nullptr;
    currentInstanceId_ = eventStack.Size() == 1 ? 0 : 
        eventStack[eventStack.Size() - 2].EventInstanceId();

    return AnalysisControl::CONTINUE;
}

void ContextBuilder::ProcessLinkerOutput(const LinkerGroup& linkers, 
    const FileOutput& output, bool overwrite)
{
    // A linker invocation can respawn itself. If this occurs we assign
    // the linker's main component to all invocations.

    for (const Invocation& invocation : linkers)
    {
        auto it = mainComponentCache_.find(invocation.EventInstanceId());

        if (!overwrite && it != mainComponentCache_.end()) {
            continue;
        }

        assert(overwrite || it == mainComponentCache_.end());

        mainComponentCache_[invocation.EventInstanceId()].Path = output.Path();
    }
}

void ContextBuilder::OnLibOutput(const LinkerGroup& linkers, const LibOutput& output)
{
    ProcessLinkerOutput(linkers, output, true);
}

void ContextBuilder::OnExecutableImageOutput(const LinkerGroup& linkers, 
    const ExecutableImageOutput& output)
{
    ProcessLinkerOutput(linkers, output, true);
}

void ContextBuilder::OnImpLibOutput(const LinkerGroup& linkers, const ImpLibOutput& output)
{
    ProcessLinkerOutput(linkers, output, false);
}

void ContextBuilder::OnCompilerInput(const Compiler& cl, const FileInput& input)
{
    auto it = mainComponentCache_.find(cl.EventInstanceId());

    if (it == mainComponentCache_.end() || !it->second.IsInput) 
    {
        auto& component = mainComponentCache_[cl.EventInstanceId()];
        
        component.Path = input.Path();
        component.IsInput = true;

        return;
    }
    else // Has a component that is an input
    {
        // Compiler invocations may have more than one input. In these
        // cases the invocation is considered to have no main component.

        it->second.Path.clear();
    }
}

void ContextBuilder::OnCompilerOutput(const Compiler& cl, const ObjOutput& output)
{
    auto it = mainComponentCache_.find(cl.EventInstanceId());

    if (it == mainComponentCache_.end()) 
    {
        auto& component = mainComponentCache_[cl.EventInstanceId()];
        
        component.Path = output.Path();
        component.IsInput = false;

        return;
    }
    else if (!it->second.IsInput)
    {
        // Compiler invocations may have more than one output. In these
        // cases the invocation is considered to have no main component.

        it->second.Path.clear();
    }
}

void ContextBuilder::OnRootActivity(const Activity& root)
{
    unsigned long long id = root.EventInstanceId();

    assert(activityContextLinks_.find(id) == activityContextLinks_.end());
    assert(contextData_.find(id) == contextData_.end());

    ContextData& newContext = contextData_[id];
    newContext.TimelineId = GetNewTimelineId();
    newContext.TimelineDescription = timelineDescriptions_[newContext.TimelineId].c_str();
    newContext.InvocationId = 0;
    newContext.InvocationDescription = L"<Unknown Invocation>";
    newContext.Tool = "<Unknown Tool>";
    newContext.Component = L"<Unknown Component>";

    activityContextLinks_.try_emplace(id, &newContext, 0);

    currentContextData_ = &newContext;
}

void ContextBuilder::OnNestedActivity(const Activity& parent, const Activity& child)
{
    unsigned long long childId = child.EventInstanceId();

    auto itParent = activityContextLinks_.find(parent.EventInstanceId());

    assert(itParent != activityContextLinks_.end());
    assert(activityContextLinks_.find(childId) == activityContextLinks_.end());

    ContextData* propagatedContext = itParent->second.LinkedContext;

    itParent->second.TimelineReuseCount++;

    activityContextLinks_.try_emplace(child.EventInstanceId(), propagatedContext, 0);

    currentContextData_ = propagatedContext;
}

void ContextBuilder::OnInvocation(const Invocation& invocation)
{
    unsigned long long id = invocation.EventInstanceId();

    auto itContextLink = activityContextLinks_.find(id);

    assert(itContextLink != activityContextLinks_.end());

    ContextData* context = itContextLink->second.LinkedContext;

    ContextData& newContext = contextData_[id];
    newContext.TimelineId = context->TimelineId;
    newContext.TimelineDescription = context->TimelineDescription;

    newContext.Tool = invocation.Type() == Invocation::Type::LINK ?
        "Link" : "CL";

    newContext.InvocationId = invocation.InvocationId();
        
    const wchar_t* wTool = invocation.Type() == Invocation::Type::LINK ?
        L"Link" : L"CL";

    std::wstring invocationIdString = std::to_wstring(invocation.InvocationId());

    unsigned long long instanceId = invocation.EventInstanceId();

    auto it = mainComponentCache_.find(instanceId);

    if (it == mainComponentCache_.end() || it->second.Path.empty()) 
    {
        std::wstring component = L"<" + std::wstring{wTool} + L" Invocation " + invocationIdString + L" Info>";
        newContext.Component = CacheString(activeComponents_, instanceId, std::move(component));

        std::wstring invocationDescription = std::wstring{wTool} + L" Invocation " + invocationIdString;
        newContext.InvocationDescription = CacheString(invocationDescriptions_, instanceId, std::move(invocationDescription));
    }
    else
    {
        newContext.Component = it->second.Path.c_str();

        std::wstring invocationDescription = std::wstring{wTool} + L" Invocation " + invocationIdString + 
            L" (" + newContext.Component + L")";
        newContext.InvocationDescription = CacheString(invocationDescriptions_, instanceId, std::move(invocationDescription));
    }

    itContextLink->second.LinkedContext = &newContext;
    currentContextData_ = &newContext;
}
    
void ContextBuilder::OnCompilerPass(const Compiler& cl, const CompilerPass& pass)
{
    ProcessParallelismForkPoint(cl, pass);

    auto* path = pass.InputSourcePath();

    if (path == nullptr) {
        path = pass.OutputObjectPath();
    }

    currentContextData_->Component = 
        CacheString(activeComponents_, pass.EventInstanceId(), path);
}

void ContextBuilder::OnC2Thread(const C2DLL& c2, const Activity& threadOwner, 
    const Thread& thread)
{
    ProcessParallelismForkPoint(threadOwner, thread);
}

void ContextBuilder::ProcessParallelismForkPoint(const Activity& parent, 
    const Activity& child)
{
    unsigned long long parentId = parent.EventInstanceId();

    auto itParentLink = activityContextLinks_.find(parentId);
    assert(itParentLink != activityContextLinks_.end());

    ProcessParallelismForkPoint(itParentLink->second, child);
}

void ContextBuilder::ProcessParallelismForkPoint(ContextLink& parentContextLink, 
    const Activity& child)
{
    unsigned long long id = child.EventInstanceId();
        
    assert(activityContextLinks_.find(id) == activityContextLinks_.end());
    assert(contextData_.find(id) == contextData_.end());

    ContextData* parentContext = parentContextLink.LinkedContext;

    ContextData& newContext = contextData_[id];
    newContext.InvocationId = parentContext->InvocationId;
    newContext.InvocationDescription = parentContext->InvocationDescription;
    newContext.Tool = parentContext->Tool;
    newContext.Component = parentContext->Component;

    if (parentContextLink.TimelineReuseCount)
    {
        auto timelineId = GetNewTimelineId();
        newContext.TimelineId = timelineId;
        newContext.TimelineDescription = timelineDescriptions_[timelineId].c_str();
    }
    else 
    {
        newContext.TimelineId = parentContext->TimelineId;
        newContext.TimelineDescription = parentContext->TimelineDescription;

        parentContextLink.TimelineReuseCount++;
    }

    activityContextLinks_.try_emplace(id, &newContext, 0);

    currentContextData_ = &newContext;
}

void ContextBuilder::OnStopRootActivity(const Activity& activity)
{
    unsigned long long id = activity.EventInstanceId();

    auto it = GetContextLink(id);

    assert(it->second.TimelineReuseCount == 0);

    availableTimelineIds_.push(it->second.LinkedContext->TimelineId);

    activityContextLinks_.erase(it);
    contextData_.erase(id);
}

void ContextBuilder::OnStopNestedActivity(const Activity& parent, 
    const Activity& child)
{
    auto itParent = GetContextLink(parent.EventInstanceId());
    auto itChild = GetContextLink(child.EventInstanceId());

    assert(itChild->second.TimelineReuseCount == 0);

    ContextData* parentContext = itParent->second.LinkedContext;
    ContextData* childContext = itChild->second.LinkedContext;

    assert(parentContext && childContext);

    if (parentContext->TimelineId == childContext->TimelineId) 
    {
        assert(itParent->second.TimelineReuseCount);
        itParent->second.TimelineReuseCount--;
    }
    else {
        availableTimelineIds_.push(childContext->TimelineId);
    }

    activityContextLinks_.erase(itChild);
}

void ContextBuilder::OnStopCompilerPass(const CompilerPass& pass)
{
    activeComponents_.erase(pass.EventInstanceId());
    contextData_.erase(pass.EventInstanceId());
}

void ContextBuilder::OnStopInvocation(const Invocation& invocation)
{
    activeComponents_.erase(invocation.EventInstanceId());
    invocationDescriptions_.erase(invocation.EventInstanceId());
    contextData_.erase(invocation.EventInstanceId());
}

void ContextBuilder::OnStopC2Thread(const C2DLL& c2, const Thread& thread)
{
    contextData_.erase(thread.EventInstanceId());
}

unsigned short ContextBuilder::GetNewTimelineId()
{
    unsigned short timelineId;

    if (availableTimelineIds_.empty())
    {
        timelineId = timelineCount_++;
    }
    else
    {
        timelineId = availableTimelineIds_.top();
        availableTimelineIds_.pop();
    }

    if (timelineDescriptions_.size() == timelineId) 
    {
        timelineDescriptions_.emplace(timelineId,
            std::string{ "Timeline " } + std::to_string(timelineId));
    }

    return timelineId;
}