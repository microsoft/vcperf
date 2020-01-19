#include "BuildExplorerView.h"
#include "Utility.h"
#include "CppBuildInsightsEtw.h"
#include "PayloadBuilder.h"

AnalysisControl BuildExplorerView::OnActivity(const EventStack& eventStack, 
    const void* relogSession)
{
    if (	MatchEventInMemberFunction(eventStack.Back(), this, 
				&BuildExplorerView::OnInvocation, relogSession)

		||	MatchEventStackInMemberFunction(eventStack, this,
				&BuildExplorerView::OnCompilerPass, relogSession)

        ||	MatchEventStackInMemberFunction(eventStack, this,
				&BuildExplorerView::OnThread, relogSession))
    {
        return AnalysisControl::CONTINUE;
    }

	switch (eventStack.Back().EntityId())
	{
	case Info<LinkerPass1>::ID:
	case Info<LinkerPass2>::ID:
	case Info<LinkerPreLTCGOptRef>::ID:
	case Info<LinkerLTCG>::ID:
	case Info<LinkerOptRef>::ID:
	case Info<LinkerOptICF>::ID:
	case Info<LinkerOptLBR>::ID:
	case Info<C1DLL>::ID:
	case Info<C2DLL>::ID:
	case Info<WholeProgramAnalysis>::ID:
	case Info<CodeGeneration>::ID:
		break;

	default:
		return AnalysisControl::CONTINUE;
	}

    LogActivity(relogSession, eventStack.Back());

    return AnalysisControl::CONTINUE;
}

AnalysisControl BuildExplorerView::OnSimpleEvent(const EventStack& eventStack, 
    const void* relogSession)
{
    if (	MatchEventStackInMemberFunction(eventStack, this, 
                &BuildExplorerView::OnCommandLine, relogSession)

		||	MatchEventStackInMemberFunction(eventStack, this,
				&BuildExplorerView::OnCompilerEnvironmentVariable, relogSession)
        
        ||	MatchEventStackInMemberFunction(eventStack, this,
				&BuildExplorerView::OnLinkerEnvironmentVariable, relogSession))
    {
        return AnalysisControl::CONTINUE;
    }

    return AnalysisControl::CONTINUE;
}

void BuildExplorerView::EmitInvocationEvents(Invocation invocation, const void* relogSession)
{
    LogActivity(relogSession, invocation);

    // For start events we also log invocaton properties such as version, working directory, etc...
    ProcessStringProperty(relogSession, invocation,
        "Version", invocation.ToolVersionString());

    // Tool path is not available for earlier versions of the toolset
    if (invocation.ToolPath()) 
    {
        ProcessStringProperty(relogSession, invocation,
            "ToolPath", invocation.ToolPath());
    }

    ProcessStringProperty(relogSession, invocation,
        "WorkingDirectory", invocation.WorkingDirectory());
}

void BuildExplorerView::OnCompilerEnvironmentVariable(Compiler cl, 
    EnvironmentVariable envVar, const void* relogSession)
{
    auto* name = envVar.Name();

    if (_wcsicmp(name, L"CL") == 0)
    {
        ProcessStringProperty(relogSession, cl, "Env Var: CL", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"_CL_") == 0)
    {
        ProcessStringProperty(relogSession, cl, "Env Var: _CL_", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"INCLUDE") == 0)
    {
        ProcessStringProperty(relogSession, cl, "Env Var: INCLUDE", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"LIBPATH") == 0)
    {
        ProcessStringProperty(relogSession, cl, "Env Var: LIBPATH", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"PATH") == 0)
    {
        ProcessStringProperty(relogSession, cl, "Env Var: PATH", envVar.Value());
        return;
    }
}

void BuildExplorerView::OnLinkerEnvironmentVariable(Linker link, 
    EnvironmentVariable envVar, const void* relogSession)
{
    auto* name = envVar.Name();

    if (_wcsicmp(name, L"LINK") == 0)
    {
        ProcessStringProperty(relogSession, link, "Env Var: LINK", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"_LINK_") == 0)
    {
        ProcessStringProperty(relogSession, link, "Env Var: _LINK_", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"LIB") == 0)
    {
        ProcessStringProperty(relogSession, link, "Env Var: LIB", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"PATH") == 0)
    {
        ProcessStringProperty(relogSession, link, "Env Var: PATH", envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"TMP") == 0)
    {
        ProcessStringProperty(relogSession, link, "Env Var: TMP", envVar.Value());
        return;
    }
}

void BuildExplorerView::LogActivity(const void* relogSession, Activity a, 
    const char* activityName)
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsBuildExplorerActivity;

    auto* context = contextBuilder_->GetContextData();
    auto& td = miscellaneousCache_->GetTimingData(a);

    Payload p = PayloadBuilder<
        uint16_t, const char*, const char*, uint32_t, const wchar_t*, 
        const wchar_t*, const char*, uint32_t, uint32_t, uint32_t, uint32_t>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->InvocationDescription,
            context->Component,
            activityName,
            (uint32_t)duration_cast<milliseconds>(td.ExclusiveDuration).count(),
            (uint32_t)duration_cast<milliseconds>(td.Duration).count(),
            (uint32_t)duration_cast<milliseconds>(td.ExclusiveCPUTime).count(),
            (uint32_t)duration_cast<milliseconds>(td.CPUTime).count());

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc, a.ProcessId(), a.ThreadId(),
        a.ProcessorIndex(), a.StartTimestamp(), p.GetData(), (unsigned long)p.Size());
}


template <typename TChar>
void BuildExplorerView::ProcessStringProperty(const void* relogSession, 
    const Entity& entity, const char* name, const TChar* value)
{
    size_t len = std::char_traits<TChar>::length(value);

    constexpr size_t COMMAND_LINE_SEGMENT_LEN = 1000;

    while (len > COMMAND_LINE_SEGMENT_LEN)
    {
        TChar segment[COMMAND_LINE_SEGMENT_LEN + 1];

        memcpy(segment, value, COMMAND_LINE_SEGMENT_LEN * sizeof(TChar));
        segment[COMMAND_LINE_SEGMENT_LEN] = 0;

        LogStringPropertySegment(relogSession, entity, name, segment);

        len -= COMMAND_LINE_SEGMENT_LEN;
        value += COMMAND_LINE_SEGMENT_LEN;
    }

    LogStringPropertySegment(relogSession, entity, name, value);
}


void BuildExplorerView::LogStringPropertySegment(const void* relogSession, 
    const Entity& entity, const char* name, const char* value)
{
    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsBuildExplorerAnsiStringProperty;

    auto* context = contextBuilder_->GetContextData();

    Payload p = PayloadBuilder<
        uint16_t, const char*, const char*, uint32_t, const wchar_t*, 
        const wchar_t*, const char*, const char*>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->InvocationDescription,
            context->Component,
            name,
            value
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc,
        entity.ProcessId(), entity.ThreadId(), entity.ProcessorIndex(),
        entity.Timestamp(), p.GetData(), (unsigned long)p.Size());
}

void BuildExplorerView::LogStringPropertySegment(const void* relogSession,
    const Entity& entity, const char* name, const wchar_t* value)
{
    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsBuildExplorerUnicodeStringProperty;

    auto* context = contextBuilder_->GetContextData();

    Payload p = PayloadBuilder<
        uint16_t, const char*, const char*, uint32_t, const wchar_t*,
        const wchar_t*, const char*, const wchar_t*>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->InvocationDescription,
            context->Component,
            name,
            value
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc,
        entity.ProcessId(), entity.ThreadId(), entity.ProcessorIndex(),
        entity.Timestamp(), p.GetData(), (unsigned long)p.Size());
}