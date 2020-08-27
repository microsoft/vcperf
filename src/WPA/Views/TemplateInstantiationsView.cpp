#include "TemplateInstantiationsView.h"
#include "CppBuildInsightsEtw.h"
#include "PayloadBuilder.h"

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

namespace vcperf
{

void TemplateInstantiationsView::OnTemplateInstantiationStart( 
    const TemplateInstantiation& ti, const void* relogSession)
{
    using std::chrono::duration_cast;
    using std::chrono::microseconds;

    PCEVENT_DESCRIPTOR desc = &CppBuildInsightsTemplateInstantiationActivity_V1;

    auto tiInfo = tiCache_->GetTemplateInstantiationInfo(ti);

    bool isInfoAvailable = std::get<0>(tiInfo);

    if (!isInfoAvailable) {
        return;
    }

    auto* context = contextBuilder_->GetContextData();

    auto& td = miscellaneousCache_->GetTimingData(ti);

    const char* primaryTemplateName = std::get<1>(tiInfo);
    const char* specializationName = std::get<2>(tiInfo);

    Payload p = PayloadBuilder <uint16_t, const char*, const char*, uint32_t, const wchar_t*, const char*,
        const char*, uint32_t, uint32_t>::Build(
            context->TimelineId,
            context->TimelineDescription,
            context->Tool,
            context->InvocationId,
            context->Component,
            primaryTemplateName,
            specializationName,
            (uint32_t)duration_cast<microseconds>(td.Duration).count(),
            (uint32_t)duration_cast<microseconds>(td.WallClockTimeResponsibility).count()
        );

    InjectEvent(relogSession, &CppBuildInsightsGuid, desc,
                ti.ProcessId(), ti.ThreadId(), ti.ProcessorIndex(), 
                ti.StartTimestamp(), p.GetData(), (unsigned long)p.Size());
}

} // namespace vcperf