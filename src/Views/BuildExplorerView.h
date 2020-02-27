#pragma once

#include "VcperfBuildInsights.h"
#include "Analyzers\ContextBuilder.h"
#include "Analyzers\MiscellaneousCache.h"

namespace vcperf
{

class BuildExplorerView : public BI::IRelogger
{
public:
    BuildExplorerView(ContextBuilder* contextBuilder,
        MiscellaneousCache* miscellaneousCache) :
        contextBuilder_{contextBuilder},
        miscellaneousCache_{miscellaneousCache}
    {}

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack, 
        const void* relogSession) override
    {
        return OnActivity(eventStack, relogSession);
    }

    BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack, 
        const void* relogSession) override;

    void OnInvocation(const A::Invocation& invocation, const void* relogSession)
    {
        EmitInvocationEvents(invocation, relogSession);
    }

    void OnCompilerPass(const A::CompilerPass& pass, const void* relogSession) 
    {
        LogActivity(relogSession, pass);
    }

    void OnThread(const A::Activity& a, const A::Thread& t, const void* relogSession)
    {   
        OnThreadActivity(a, t, relogSession);
    }

    void OnCommandLine(const A::Invocation& invocation, const SE::CommandLine& commandLine, 
        const void* relogSession)
    {
        ProcessStringProperty(relogSession, invocation, "CommandLine", commandLine.Value());
    }

    void OnCompilerEnvironmentVariable(const A::Compiler& cl, const SE::EnvironmentVariable& envVar, 
        const void* relogSession);

    void OnLinkerEnvironmentVariable(const A::Linker& link, const SE::EnvironmentVariable& envVar, 
        const void* relogSession);

    BI::AnalysisControl OnEndRelogging() override
    {
        return BI::AnalysisControl::CONTINUE;
    }

private:

    BI::AnalysisControl OnActivity(const BI::EventStack& eventStack, const void* relogSession);

    void OnThreadActivity(const A::Activity& a, const A::Thread& t, const void* relogSession)
    {
        std::string activityName = a.EventName();
        activityName += "Thread";

        LogActivity(relogSession, t, activityName.c_str());
    }

    void EmitInvocationEvents(const A::Invocation& invocation, const void* relogSession);

    void LogActivity(const void* relogSession, const A::Activity& a, const char* activityName);

    void LogActivity(const void* relogSession, const A::Activity& a)
    {
        LogActivity(relogSession, a, a.EventName());
    }

    template <typename TChar>
    void ProcessStringProperty(const void* relogSession, 
        const A::Invocation& invocation, const char* name, const TChar* value);

    void LogStringPropertySegment(const void* relogSession, 
        const A::Invocation& invocation, const char* name, const char* value);

    void LogStringPropertySegment(const void* relogSession, 
        const A::Invocation& invocation, const char* name, const wchar_t* value);

    template <typename TChar>
    void LogStringPropertySegment(const void* relogSession, const A::Invocation& invocation, 
        const char* name, const TChar* value, PCEVENT_DESCRIPTOR desc);

    std::wstring invocationInfoString_;

    ContextBuilder* contextBuilder_;
    MiscellaneousCache* miscellaneousCache_;
};

} // namespace vcperf