#pragma once

#include "CppBuildInsights.hpp"
#include "ContextBuilder.h"
#include "TimingDataCache.h"


using namespace Microsoft::Cpp::BuildInsights;

using namespace Activities;
using namespace SimpleEvents;

class BuildExplorerView : public IRelogger
{
public:
    BuildExplorerView(ContextBuilder* contextBuilder,
        TimingDataCache* timingDataCache) :
        contextBuilder_{contextBuilder},
        timingDataCache_{timingDataCache}
    {}

    AnalysisControl OnStartActivity(const EventStack& eventStack, 
        const void* relogSession) override
    {
        return OnActivity(eventStack, relogSession);
    }

    AnalysisControl OnSimpleEvent(const EventStack& eventStack, 
        const void* relogSession) override;

    void OnInvocation(Invocation invocation, const void* relogSession)
    {
        EmitInvocationEvents(invocation, relogSession);
    }

    void OnCompilerPass(CompilerPass pass, const void* relogSession) 
    {
        LogActivity(relogSession, pass);
    }

    void OnThread(Activity a, Thread t, const void* relogSession)
    {   
        OnThreadActivity(a, t, relogSession);
    }

    void OnCommandLine(Invocation invocation, CommandLine commandLine, 
        const void* relogSession)
    {
        OnInvocationProperty(invocation, "CommandLine", commandLine.Value(), relogSession);
    }

    void OnCompilerEnvironmentVariable(Compiler cl, EnvironmentVariable envVar, 
        const void* relogSession);

    void OnLinkerEnvironmentVariable(Linker link, EnvironmentVariable envVar, 
        const void* relogSession);

    AnalysisControl OnEndRelogging() override
    {
        return AnalysisControl::CONTINUE;
    }

private:

    AnalysisControl OnActivity(const EventStack& eventStack, const void* relogSession);

    void OnThreadActivity(Activity a, Thread t, const void* relogSession)
    {
        std::string activityName = a.EntityName();
        activityName += "Thread";

        LogActivity(relogSession, t, activityName.c_str());
    }

    template <typename TChar>
    void OnInvocationProperty(const Invocation& invocation, const char* name, 
        const TChar* value, const void* relogSession)
    {
        ProcessStringProperty(relogSession, invocation, name, value);
    }

    void EmitInvocationEvents(Invocation invocation, const void* relogSession);

    void LogActivity(const void* relogSession, Activity a, const char* activityName);

    void LogActivity(const void* relogSession, Activity a)
    {
        LogActivity(relogSession, a, a.EntityName());
    }

    template <typename TChar>
    void ProcessStringProperty(const void* relogSession, 
        const Entity& entity, const char* name, const TChar* value);

    void LogStringPropertySegment(const void* relogSession, 
        const Entity& entity, const char* name, const char* value);

    void LogStringPropertySegment(const void* relogSession, 
        const Entity& entity, const char* name, const wchar_t* value);

    std::wstring invocationInfoString_;

    ContextBuilder* contextBuilder_;
    TimingDataCache* timingDataCache_;
};