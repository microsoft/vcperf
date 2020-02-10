#include "FunctionsView.h"

#include "CppBuildInsightsEtw.h"

#include "PayloadBuilder.h"

enum class EventId
{
    FORCE_INLINEE = 1
};

AnalysisControl FunctionsView::OnActivity(const EventStack& eventStack, const void* relogSession)
{
    // We avoid emitting functions that take less than 100 milliseconds to optimize
    // in order to limit the size of the dataset that WPA has to deal with.
    if (miscellaneousCache_->GetTimingData(eventStack.Back()).Duration < std::chrono::milliseconds(100)) {
        return AnalysisControl::CONTINUE;
    }

    MatchEventStackInMemberFunction(eventStack, this, &FunctionsView::EmitFunctionActivity, relogSession);

    return AnalysisControl::CONTINUE;
}

AnalysisControl FunctionsView::OnSimpleEvent(const EventStack& eventStack, const void* relogSession)
{
    // We avoid emitting functions that take less than 100 milliseconds to optimize
    // in order to limit the size of the dataset that WPA has to deal with.
    if (    eventStack.Size() >= 2 
        &&  miscellaneousCache_->GetTimingData(eventStack[eventStack.Size()-2]).Duration < std::chrono::milliseconds(100)) 
    {
        return AnalysisControl::CONTINUE;
    }

    MatchEventStackInMemberFunction(eventStack, this, &FunctionsView::EmitFunctionForceInlinee, relogSession);

    return AnalysisControl::CONTINUE;
}

void FunctionsView::EmitFunctionActivity(Function func, const void* relogSession)
{
    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsFunctionActivity;

    auto* context = contextBuilder_->GetContextData();

    auto& td = miscellaneousCache_->GetTimingData(func);

    Payload p = PayloadBuilder<uint16_t, const char*, const char*, uint32_t, 
        const wchar_t*, const char*, const char*, uint32_t>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->Component,
            func.Name(),
            "CodeGeneration",
            (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(td.Duration).count()
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc, 
        func.ProcessId(), func.ThreadId(), func.ProcessorIndex(),
        func.StartTimestamp(), p.GetData(), (unsigned long)p.Size());

    desc = &CppBuildInsightsFunctionActivity_Extended1;

    Payload p2 = PayloadBuilder<uint64_t>::Build(func.InstanceId());

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc, 
        func.ProcessId(), func.ThreadId(), func.ProcessorIndex(),
        func.StartTimestamp(), p2.GetData(), (unsigned long)p2.Size());
}

void FunctionsView::EmitFunctionForceInlinee(const Function& func, 
    const ForceInlinee& forceInlinee, const void* relogSession)
{
    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsFunctionSimpleEvent;

    auto* context = contextBuilder_->GetContextData();

    Payload p = PayloadBuilder<uint16_t, const char*, const char*, uint32_t,
        const wchar_t*, const char*, const char*, const char*, const char*, 
        const char*, const char*, int32_t>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->Component,
            func.Name(),
            "CodeGeneration",
            "ForceInlinee",
            "",
            forceInlinee.Name(),
            "",
            forceInlinee.Size()
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc,
        forceInlinee.ProcessId(), forceInlinee.ThreadId(), forceInlinee.ProcessorIndex(),
        forceInlinee.Timestamp(), p.GetData(), (unsigned long)p.Size());

    desc = &CppBuildInsightsFunctionSimpleEvent_Extended1;

    Payload p2 = PayloadBuilder<uint64_t, uint16_t>::Build(func.InstanceId(),
        static_cast<uint16_t>(EventId::FORCE_INLINEE));

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc, 
        forceInlinee.ProcessId(), forceInlinee.ThreadId(), forceInlinee.ProcessorIndex(),
        forceInlinee.Timestamp(), p2.GetData(), (unsigned long)p2.Size());
}