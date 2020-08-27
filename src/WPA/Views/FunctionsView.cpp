#include "FunctionsView.h"

#include "CppBuildInsightsEtw.h"

#include "PayloadBuilder.h"

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

namespace vcperf
{

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
    using namespace std::chrono;

    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsFunctionActivity_V1;

    auto* context = contextBuilder_->GetContextData();

    auto& td = miscellaneousCache_->GetTimingData(func);

    Payload p = PayloadBuilder<uint16_t, const char*, const char*, uint32_t, 
        const wchar_t*, uint64_t, const char*, const char*, uint32_t, uint32_t>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->Component,
            func.EventInstanceId(),
            func.Name(),
            "CodeGeneration",
            (uint32_t)duration_cast<milliseconds>(td.Duration).count(),
            (uint32_t)duration_cast<milliseconds>(td.WallClockTimeResponsibility).count()
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc, 
        func.ProcessId(), func.ThreadId(), func.ProcessorIndex(),
        func.StartTimestamp(), p.GetData(), (unsigned long)p.Size());
}

void FunctionsView::EmitFunctionForceInlinee(const Function& func, 
    const ForceInlinee& forceInlinee, const void* relogSession)
{
    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsFunctionSimpleEvent_V1;

    auto* context = contextBuilder_->GetContextData();

    Payload p = PayloadBuilder<uint16_t, const char*, const char*, uint32_t,
        const wchar_t*, uint64_t, const char*, const char*, uint16_t, const char*, 
        const char*, int32_t>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->Component,
            func.EventInstanceId(),
            func.Name(),
            "CodeGeneration",
            static_cast<uint16_t>(EventId::FORCE_INLINEE),
            "ForceInlinee",
            forceInlinee.Name(),
            forceInlinee.Size()
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc,
        forceInlinee.ProcessId(), forceInlinee.ThreadId(), forceInlinee.ProcessorIndex(),
        forceInlinee.Timestamp(), p.GetData(), (unsigned long)p.Size());
}

} // namespace vcperf