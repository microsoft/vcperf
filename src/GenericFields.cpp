#include "VcperfBuildInsights.h"
#include "GenericFields.h"
#include "PayloadBuilder.h"
#include "CppBuildInsightsEtw.h"
#include "Utility.h"

using namespace Microsoft::Cpp::BuildInsights;

namespace vcperf
{

template <typename TField>
void LogGenericField(TField value, PCEVENT_DESCRIPTOR desc, const Event& e, const void* relogSession)
{
    Payload p = PayloadBuilder<TField>::Build(value);
   
    InjectEvent(relogSession, &CppBuildInsightsGuid, desc, 
        e.ProcessId(), e.ThreadId(), e.ProcessorIndex(),
        e.Timestamp(), p.GetData(), (unsigned long)p.Size());
}

void LogGenericStringField(const char* value, const Event& e, const void* relogSession)
{
    LogGenericField(value, &CppBuildInsightsAnsiStringGenericField, e, relogSession);
}

void LogGenericStringField(const wchar_t* value, const Event& e, const void* relogSession)
{
    LogGenericField(value, &CppBuildInsightsUnicodeStringGenericField, e, relogSession);
}

void LogGenericUTF8StringField(const char* value, const Event& e, const void* relogSession)
{
    LogGenericField(value, &CppBuildInsightsUTF8StringGenericField, e, relogSession);
}

void LogGenericIntegerField(int64_t value, const Event& e, const void* relogSession)
{
    LogGenericField(value, &CppBuildInsightsIntegerGenericField, e, relogSession);
}

} // namespace vcperf