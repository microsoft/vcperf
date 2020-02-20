#pragma once

#include <cwchar>
#include <cstdint>
#include "CppBuildInsights.hpp"

using namespace Microsoft::Cpp::BuildInsights;

void LogGenericStringField(const char* value, const Event& e, const void* relogSession);
void LogGenericStringField(const wchar_t* value, const Event& e, const void* relogSession);
void LogGenericUTF8StringField(const char* value, const Event& e, const void* relogSession);
void LogGenericIntegerField(int64_t value, const Event& e, const void* relogSession);