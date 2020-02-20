#pragma once

#include "CppBuildInsights.hpp"
#include "ContextBuilder.h"
#include "MiscellaneousCache.h"


using namespace Microsoft::Cpp::BuildInsights;

using namespace Activities;
using namespace SimpleEvents;

class BuildExplorerView : public IRelogger
{
public:
    BuildExplorerView(ContextBuilder* contextBuilder,
        MiscellaneousCache* miscellaneousCache) :
        contextBuilder_{contextBuilder},
        miscellaneousCache_{miscellaneousCache}
    {}

    AnalysisControl OnStartActivity(const EventStack& eventStack, 
        const void* relogSession) override
    {
        return OnActivity(eventStack, relogSession);
    }

    AnalysisControl OnSimpleEvent(const EventStack& eventStack, 
        const void* relogSession) override;

    void OnInvocation(const Invocation& invocation, const void* relogSession)
    {
        EmitInvocationEvents(invocation, relogSession);
    }

    void OnCompilerPass(const CompilerPass& pass, const void* relogSession) 
    {
        LogActivity(relogSession, pass);
    }

    void OnThread(const Activity& a, const Thread& t, const void* relogSession)
    {   
        OnThreadActivity(a, t, relogSession);
    }

    void OnCommandLine(const Invocation& invocation, const CommandLine& commandLine, 
        const void* relogSession)
    {
        ProcessStringProperty(relogSession, invocation, "CommandLine", commandLine.Value());
    }

    void OnCompilerEnvironmentVariable(const Compiler& cl, const EnvironmentVariable& envVar, 
        const void* relogSession);

    void OnLinkerEnvironmentVariable(const Linker& link, const EnvironmentVariable& envVar, 
        const void* relogSession);

    AnalysisControl OnEndRelogging() override
    {
        return AnalysisControl::CONTINUE;
    }

private:

    AnalysisControl OnActivity(const EventStack& eventStack, const void* relogSession);

    void OnThreadActivity(const Activity& a, const Thread& t, const void* relogSession)
    {
        std::string activityName = a.EventName();
        activityName += "Thread";

        LogActivity(relogSession, t, activityName.c_str());
    }

    void EmitInvocationEvents(const Invocation& invocation, const void* relogSession);

    void LogActivity(const void* relogSession, const Activity& a, const char* activityName);

    void LogActivity(const void* relogSession, const Activity& a)
    {
        LogActivity(relogSession, a, a.EventName());
    }

    template <typename TChar>
    void ProcessStringProperty(const void* relogSession, 
        const Invocation& invocation, const char* name, const TChar* value);

    void LogStringPropertySegment(const void* relogSession, 
        const Invocation& invocation, const char* name, const char* value);

    void LogStringPropertySegment(const void* relogSession, 
        const Invocation& invocation, const char* name, const wchar_t* value);

    template <typename TChar>
    void LogStringPropertySegment(const void* relogSession, const Invocation& invocation, 
        const char* name, const TChar* value, PCEVENT_DESCRIPTOR desc);

    std::wstring invocationInfoString_;

    ContextBuilder* contextBuilder_;
    MiscellaneousCache* miscellaneousCache_;
};