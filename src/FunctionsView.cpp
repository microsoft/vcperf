#include "FunctionsView.h"

#include "CppBuildInsightsEtw.h"

#include "PayloadBuilder.h"

AnalysisControl FunctionsView::OnActivity(const EventStack& eventStack, const void* relogSession)
{
    // We avoid emitting functions that take less than 100 milliseconds to optimize
    // in order to limit the size of the dataset that WPA has to deal with.
    if (timingDataCache_->GetTimingData(eventStack.Back()).Duration < std::chrono::milliseconds(100)) {
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
        &&  timingDataCache_->GetTimingData(eventStack[eventStack.Size()-2]).Duration < std::chrono::milliseconds(100)) 
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

    auto& td = timingDataCache_->GetTimingData(func);

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
}

void FunctionsView::EmitFunctionForceInlinee(Function func, ForceInlinee forceInlinee, 
    const void* relogSession)
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
            "Name",
            forceInlinee.Name(),
            "Size",
            forceInlinee.Size()
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc,
        forceInlinee.ProcessId(), forceInlinee.ThreadId(), forceInlinee.ProcessorIndex(),
        forceInlinee.Timestamp(), p.GetData(), (unsigned long)p.Size());
}