#pragma once

#include <cwchar>
#include <cstdint>
#include "VcperfBuildInsights.h"

namespace vcperf
{

void LogGenericStringField(const char* value, const BI::Event& e, const void* relogSession);
void LogGenericStringField(const wchar_t* value, const BI::Event& e, const void* relogSession);
void LogGenericUTF8StringField(const char* value, const BI::Event& e, const void* relogSession);
void LogGenericIntegerField(int64_t value, const BI::Event& e, const void* relogSession);

} // namespace vcperf