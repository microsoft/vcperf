#include "BuildExplorerView.h"
#include "Utility.h"
#include "CppBuildInsightsEtw.h"
#include "PayloadBuilder.h"

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

namespace vcperf
{

AnalysisControl BuildExplorerView::OnActivity(const EventStack& eventStack, 
    const void* relogSession)
{
    if (    MatchEventInMemberFunction(eventStack.Back(), this, 
                &BuildExplorerView::OnInvocation, relogSession)

        ||  MatchEventStackInMemberFunction(eventStack, this,
                &BuildExplorerView::OnCompilerPass, relogSession)

        ||  MatchEventStackInMemberFunction(eventStack, this,
                &BuildExplorerView::OnThread, relogSession))
    {
        return AnalysisControl::CONTINUE;
    }

    switch (eventStack.Back().EventId())
    {
    case Info<Pass1>::ID:
    case Info<Pass2>::ID:
    case Info<PreLTCGOptRef>::ID:
    case Info<LTCG>::ID:
    case Info<OptRef>::ID:
    case Info<OptICF>::ID:
    case Info<OptLBR>::ID:
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
    if (    MatchEventStackInMemberFunction(eventStack, this, 
                &BuildExplorerView::OnCommandLine, relogSession)

        ||  MatchEventStackInMemberFunction(eventStack, this,
                &BuildExplorerView::OnCompilerEnvironmentVariable, relogSession)
        
        ||  MatchEventStackInMemberFunction(eventStack, this,
                &BuildExplorerView::OnLinkerEnvironmentVariable, relogSession))
    {
        return AnalysisControl::CONTINUE;
    }

    return AnalysisControl::CONTINUE;
}

void BuildExplorerView::EmitInvocationEvents(const Invocation& invocation, 
    const void* relogSession)
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

void BuildExplorerView::OnCompilerEnvironmentVariable(const Compiler& cl, 
    const EnvironmentVariable& envVar, const void* relogSession)
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

void BuildExplorerView::OnLinkerEnvironmentVariable(const Linker& link, 
    const EnvironmentVariable& envVar, const void* relogSession)
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

void BuildExplorerView::LogActivity(const void* relogSession, const Activity& a, 
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
    const Invocation& invocation, const char* name, const TChar* value)
{
    size_t len = std::char_traits<TChar>::length(value);

    constexpr size_t COMMAND_LINE_SEGMENT_LEN = 1000;

    while (len > COMMAND_LINE_SEGMENT_LEN)
    {
        TChar segment[COMMAND_LINE_SEGMENT_LEN + 1];

        memcpy(segment, value, COMMAND_LINE_SEGMENT_LEN * sizeof(TChar));
        segment[COMMAND_LINE_SEGMENT_LEN] = 0;

        LogStringPropertySegment(relogSession, invocation, name, segment);

        len -= COMMAND_LINE_SEGMENT_LEN;
        value += COMMAND_LINE_SEGMENT_LEN;
    }

    LogStringPropertySegment(relogSession, invocation, name, value);
}


void BuildExplorerView::LogStringPropertySegment(const void* relogSession, 
    const Invocation& invocation, const char* name, const char* value)
{
    LogStringPropertySegment(relogSession, invocation, name, value, 
        &CppBuildInsightsBuildExplorerAnsiStringProperty);
}

void BuildExplorerView::LogStringPropertySegment(const void* relogSession,
    const Invocation& invocation, const char* name, const wchar_t* value)
{
    LogStringPropertySegment(relogSession, invocation, name, value, 
        &CppBuildInsightsBuildExplorerUnicodeStringProperty);
}

template <typename TChar>
void BuildExplorerView::LogStringPropertySegment(const void* relogSession,
    const Invocation& invocation, const char* name, const TChar* value,
    PCEVENT_DESCRIPTOR desc)
{
    auto* context = contextBuilder_->GetContextData();

    Payload p = PayloadBuilder<
        uint16_t, const char*, const char*, uint32_t, const wchar_t*,
        const wchar_t*, const char*, const TChar*>::Build(
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
        invocation.ProcessId(), invocation.ThreadId(), invocation.ProcessorIndex(),
        invocation.StartTimestamp(), p.GetData(), (unsigned long)p.Size());
}

} // namespace vcperf