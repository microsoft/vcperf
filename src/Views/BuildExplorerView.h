#pragma once

#include "VcperfBuildInsights.h"
#include "Analyzers\ContextBuilder.h"
#include "Analyzers\MiscellaneousCache.h"
#include "Analyzers\RestartedLinkerDetector.h"

namespace vcperf
{

class BuildExplorerView : public BI::IRelogger
{
public:
    BuildExplorerView(
        const RestartedLinkerDetector* restartedLinkerDetector,
        ContextBuilder* contextBuilder,
        MiscellaneousCache* miscellaneousCache) :
        restartedLinkerDetector_{restartedLinkerDetector},
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

    void OnCompilerPass(const A::Compiler& cl, const A::CompilerPass& pass, const void* relogSession) 
    {
        LogActivity(relogSession, pass, cl);
    }

    void OnThread(const A::Invocation& invocation, const A::Activity& a, const A::Thread& t, 
        const void* relogSession)
    {   
        OnThreadActivity(invocation, a, t, relogSession);
    }

    void OnCommandLine(const A::Invocation& invocation, const SE::CommandLine& commandLine, 
        const void* relogSession)
    {
        ProcessStringProperty(relogSession, invocation, commandLine, 
            "CommandLine", commandLine.Value());
    }

    void OnGeneralActivity(const A::Invocation& invocation, const A::Activity& a, 
        const void* relogSession)
    {
        LogActivity(relogSession, a, invocation);
    }

    void OnCompilerEnvironmentVariable(const A::Compiler& cl, const SE::EnvironmentVariable& envVar, 
        const void* relogSession);

    void OnLinkerEnvironmentVariable(const A::Linker& link, const SE::EnvironmentVariable& envVar, 
        const void* relogSession);

    const char* ClassifyInvocation(const A::Invocation& invocation);
    void EmitClassification(const A::Invocation& invocation, const BI::Event& subjectEvent, 
        const void* relogSession);

    BI::AnalysisControl OnEndRelogging() override
    {
        return BI::AnalysisControl::CONTINUE;
    }

private:

    BI::AnalysisControl OnActivity(const BI::EventStack& eventStack, const void* relogSession);

    void OnThreadActivity(const A::Invocation& invocation, const A::Activity& a, const A::Thread& t, 
        const void* relogSession)
    {
        std::string activityName = a.EventName();
        activityName += "Thread";

        LogActivity(relogSession, t, invocation, activityName.c_str());
    }

    void EmitInvocationEvents(const A::Invocation& invocation, const void* relogSession);

    void LogActivity(const void* relogSession, const A::Activity& a, const A::Invocation& invocation,
        const char* activityName);

    void LogActivity(const void* relogSession, const A::Activity& a, const A::Invocation& invocation)
    {
        LogActivity(relogSession, a, invocation, a.EventName());
    }

    template <typename TChar>
    void ProcessStringProperty(const void* relogSession, 
        const A::Invocation& invocation, const BI::Event& e, 
        const char* name, const TChar* value);

    void LogStringPropertySegment(const void* relogSession, 
        const A::Invocation& invocation, const BI::Event& e, 
        const char* name, const char* value);

    void LogStringPropertySegment(const void* relogSession, 
        const A::Invocation& invocation, const BI::Event& e, 
        const char* name, const wchar_t* value);

    template <typename TChar>
    void LogStringPropertySegment(const void* relogSession, 
        const A::Invocation& invocation, const BI::Event& e,
        const char* name, const TChar* value, PCEVENT_DESCRIPTOR desc);

    std::wstring invocationInfoString_;

    // This is a pointer to a RestartedLinkerDetector analyzer located
    // earlier in the analysis chain. It will be used by BuildExplorerView
    // to determine whether a linker has been restarted.
    const RestartedLinkerDetector* restartedLinkerDetector_;
    ContextBuilder* contextBuilder_;
    MiscellaneousCache* miscellaneousCache_;
};

} // namespace vcperf