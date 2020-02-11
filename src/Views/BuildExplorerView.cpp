#include "BuildExplorerView.h"
#include "Utility.h"
#include "CppBuildInsightsEtw.h"
#include "PayloadBuilder.h"
#include "GenericFields.h"

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
    case EVENT_ID_PASS1:
    case EVENT_ID_PASS2:
    case EVENT_ID_PRE_LTCG_OPT_REF:
    case EVENT_ID_LTCG:
    case EVENT_ID_OPT_REF:
    case EVENT_ID_OPT_ICF:
    case EVENT_ID_OPT_LBR:
    case EVENT_ID_C1_DLL:
    case EVENT_ID_C2_DLL:
    case EVENT_ID_WHOLE_PROGRAM_ANALYSIS:
    case EVENT_ID_CODE_GENERATION:
        break;

    default:
        return AnalysisControl::CONTINUE;
    }

    MatchEventStackInMemberFunction(eventStack, this, 
	    &BuildExplorerView::OnGeneralActivity, relogSession);

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
    LogActivity(relogSession, invocation, invocation);

    // For start events we also log invocaton properties such as version, working directory, etc...
    ProcessStringProperty(relogSession, invocation, invocation,
        "Version", invocation.ToolVersionString());

    // Tool path is not available for earlier versions of the toolset
    if (invocation.ToolPath()) 
    {
        ProcessStringProperty(relogSession, invocation, invocation,
            "ToolPath", invocation.ToolPath());
    }

    ProcessStringProperty(relogSession, invocation, invocation,
        "WorkingDirectory", invocation.WorkingDirectory());
}

void BuildExplorerView::OnCompilerEnvironmentVariable(const Compiler& cl, 
    const EnvironmentVariable& envVar, const void* relogSession)
{
    auto* name = envVar.Name();

    if (_wcsicmp(name, L"CL") == 0)
    {
        ProcessStringProperty(relogSession, cl, envVar, "Env Var: CL", 
            envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"_CL_") == 0)
    {
        ProcessStringProperty(relogSession, cl, envVar, "Env Var: _CL_", 
            envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"INCLUDE") == 0)
    {
        ProcessStringProperty(relogSession, cl, envVar, "Env Var: INCLUDE", 
            envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"LIBPATH") == 0)
    {
        ProcessStringProperty(relogSession, cl, envVar, "Env Var: LIBPATH", 
            envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"PATH") == 0)
    {
        ProcessStringProperty(relogSession, cl, envVar, "Env Var: PATH", 
            envVar.Value());
        return;
    }
}

void BuildExplorerView::OnLinkerEnvironmentVariable(const Linker& link, 
    const EnvironmentVariable& envVar, const void* relogSession)
{
    auto* name = envVar.Name();

    if (_wcsicmp(name, L"LINK") == 0) 
    {
        ProcessStringProperty(relogSession, link, envVar, "Env Var: LINK", 
            envVar.Value());
        return;
    }

    if (_wcsicmp(name, L"_LINK_") == 0) 
    {
        ProcessStringProperty(relogSession, link, envVar, "Env Var: _LINK_", 
            envVar.Value());
        return;
    }
    
    if (_wcsicmp(name, L"LIB") == 0) 
    {
        ProcessStringProperty(relogSession, link, envVar, "Env Var: LIB", 
            envVar.Value());
        return;
    }
    
    if (_wcsicmp(name, L"PATH") == 0) 
    {
        ProcessStringProperty(relogSession, link, envVar, "Env Var: PATH", 
            envVar.Value());
        return;
    }
    
    if (_wcsicmp(name, L"TMP") == 0) 
    {
        ProcessStringProperty(relogSession, link, envVar, "Env Var: TMP", 
            envVar.Value());
        return;
    }
}

// This function classifies a given invocation into one of three categories:
// a compiler, a linker, or a restarted linker. 
const char* BuildExplorerView::ClassifyInvocation(const Invocation& invocation)
{
    if (invocation.Type() == Invocation::Type::CL) {
        return "Compiler";
    }

    // We use the RestartedLinkerDetector analyzer from 
    // RestartedLinkerDetector.h to determine whether the linker
    // has been restarted.
    if (restartedLinkerDetector_->WasLinkerRestarted(invocation)) {
        return "Restarted Linker";
    }

    return "Linker";
}

// BACKGROUND:
//
// The C++ Build Insights WPA views, such as the Build Explorer, display their
// information in table form. These tables have columns which are described in 
// the CppBuildInsightsEtw.xml ETW schema. In vcperf code, you can freely add 
// rows to these tables by calling the InjectEvent function. However, you cannot 
// add columns to them because the C++ Build Insights WPA add-in is closed-source 
// and understands a fixed ETW schema. To get around this limitation, each table
// includes 'generic' columns that you can use to fill with your own data.
// Generic columns are filled by emitting a Generic Field event immediately
// after calling InjectEvent for the regular row event. If the table you are
// filling supports more than 1 generic field, such as the Build Explorer view,
// you can fill them in succession by emitting 2 or 3 Generic Field events one
// after the other. 
//
// FUNCTION DESCRIPTION:
//
// This function emits the classification of a given invocation inside a
// Generic Field event. This allows grouping each row in the Build Explorer view
// based on whether it comes from a compiler, a linker, or a restarted linker.
// Use these groupings to quickly single out restarted linkers when viewing a 
// trace in WPA.
void BuildExplorerView::EmitClassification(const Invocation& invocation, 
    const Event& subjectEntity, const void* relogSession)
{
    LogGenericStringField(ClassifyInvocation(invocation), subjectEntity, relogSession);
}

void BuildExplorerView::LogActivity(const void* relogSession, const Activity& a, 
    const Invocation& invocation, const char* activityName)
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

    // A generic field event needs to be emitted after each activity.
    // This event contains the classification of the invocation that 
    // encapsulates this activity.
    EmitClassification(invocation, a, relogSession);
}


template <typename TChar>
void BuildExplorerView::ProcessStringProperty(const void* relogSession, 
    const Invocation& invocation, const Event& e, const char* name, 
    const TChar* value)
{
    size_t len = std::char_traits<TChar>::length(value);

    constexpr size_t COMMAND_LINE_SEGMENT_LEN = 1000;

    while (len > COMMAND_LINE_SEGMENT_LEN)
    {
        TChar segment[COMMAND_LINE_SEGMENT_LEN + 1];

        memcpy(segment, value, COMMAND_LINE_SEGMENT_LEN * sizeof(TChar));
        segment[COMMAND_LINE_SEGMENT_LEN] = 0;

        LogStringPropertySegment(relogSession, invocation, e, name, segment);

        len -= COMMAND_LINE_SEGMENT_LEN;
        value += COMMAND_LINE_SEGMENT_LEN;
    }

    LogStringPropertySegment(relogSession, invocation, e, name, value);
}


void BuildExplorerView::LogStringPropertySegment(const void* relogSession, 
    const Invocation& invocation, const Event& e, const char* name, 
    const char* value)
{
    LogStringPropertySegment(relogSession, invocation, e, name, value, 
        &CppBuildInsightsBuildExplorerAnsiStringProperty);
}

void BuildExplorerView::LogStringPropertySegment(const void* relogSession,
    const Invocation& invocation, const Event& e, const char* name, 
    const wchar_t* value)
{
    LogStringPropertySegment(relogSession, invocation, e, name, value, 
        &CppBuildInsightsBuildExplorerUnicodeStringProperty);
}

template <typename TChar>
void BuildExplorerView::LogStringPropertySegment(const void* relogSession,
    const Invocation& invocation, const Event& e, const char* name, 
    const TChar* value, PCEVENT_DESCRIPTOR desc)
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
        e.ProcessId(), e.ThreadId(), e.ProcessorIndex(),
        e.Timestamp(), p.GetData(), (unsigned long)p.Size());

    // A generic field event needs to be emitted after each property segment.
    // This event contains the classification of the invocation that this
    // property belongs to.
    EmitClassification(invocation, e, relogSession);
}

} // namespace vcperf
