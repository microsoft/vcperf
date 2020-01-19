#include <Windows.h>
#include "Commands.h"

#include <iostream>

#include "CppBuildInsights.hpp"

#include "ExpensiveTemplateInstantiationCache.h"
#include "ContextBuilder.h"
#include "MiscellaneousCache.h"
#include "BuildExplorerView.h"
#include "FunctionsView.h"
#include "FilesView.h"
#include "TemplateInstantiationsView.h"

using namespace Microsoft::Cpp::BuildInsights;

const wchar_t* ResultCodeToString(BUILD_INSIGHTS_RESULT_CODE rc)
{
    switch (rc)
    {
    case BUILD_INSIGHTS_RESULT_CODE_SUCCESS:
        return L"SUCCESS";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_ANALYSIS_ERROR:
        return L"FAILURE_ANALYSIS_ERROR";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_CANCELLED:
        return L"FAILURE_CANCELLED";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_INVALID_INPUT_LOG_FILE:
        return L"FAILURE_INVALID_INPUT_LOG_FILE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_INVALID_OUTPUT_LOG_FILE:
        return L"FAILURE_INVALID_OUTPUT_LOG_FILE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_MISSING_ANALYSIS_CALLBACK:
        return L"FAILURE_MISSING_ANALYSIS_CALLBACK";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_MISSING_RELOG_CALLBACK:
        return L"FAILURE_MISSING_RELOG_CALLBACK";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_OPEN_INPUT_TRACE:
        return L"FAILURE_OPEN_INPUT_TRACE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_PROCESS_TRACE:
        return L"FAILURE_PROCESS_TRACE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_START_RELOGGER:
        return L"FAILURE_START_RELOGGER";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_NO_RELOGGER:
        return L"FAILURE_NO_RELOGGER";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_DROPPED_EVENTS:
        return L"FAILURE_DROPPED_EVENTS";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_UNSUPPORTED_OS:
        return L"FAILURE_UNSUPPORTED_OS";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_INVALID_OUTPUT_REPRO_FILE:
        return L"FAILURE_INVALID_OUTPUT_REPRO_FILE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_OPEN_FILE:
        return L"FAILURE_OPEN_FILE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_WRITE_FILE:
        return L"FAILURE_WRITE_FILE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_NO_INVOCATION_TOOL_PATH:
        return L"FAILURE_NO_INVOCATION_TOOL_PATH";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_NO_INVOCATION_WORKING_DIRECTORY:
        return L"FAILURE_NO_INVOCATION_WORKING_DIRECTORY";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_NO_INVOCATION_ENVIRONMENT_VARIABLES:
        return L"FAILURE_NO_INVOCATION_ENVIRONMENT_VARIABLES";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_NO_INVOCATION_COMMAND_LINE:
        return L"FAILURE_NO_INVOCATION_COMMAND_LINE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_INVALID_TRACING_SESSION_NAME:
        return L"FAILURE_INVALID_TRACING_SESSION_NAME";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_INSUFFICIENT_PRIVILEGES:
        return L"FAILURE_INSUFFICIENT_PRIVILEGES";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_GENERATE_GUID:
        return L"FAILURE_GENERATE_GUID";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_OBTAINING_TEMP_DIRECTORY:
        return L"FAILURE_OBTAINING_TEMP_DIRECTORY";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_CREATE_TEMPORARY_DIRECTORY:
        return L"FAILURE_CREATE_TEMPORARY_DIRECTORY";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_START_SYSTEM_TRACE:
        return L"FAILURE_START_SYSTEM_TRACE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_START_MSVC_TRACE:
        return L"FAILURE_START_MSVC_TRACE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_STOP_MSVC_TRACE:
        return L"FAILURE_STOP_MSVC_TRACE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_STOP_SYSTEM_TRACE:
        return L"FAILURE_STOP_SYSTEM_TRACE";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_SESSION_DIRECTORY_RESOLUTION:
        return L"FAILURE_SESSION_DIRECTORY_RESOLUTION";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_SYSTEM_TRACE_FILE_NOT_FOUND:
        return L"FAILURE_SYSTEM_TRACE_FILE_NOT_FOUND";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_MSVC_TRACE_FILE_NOT_FOUND:
        return L"FAILURE_MSVC_TRACE_FILE_NOT_FOUND";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_MERGE_TRACES:
        return L"FAILURE_MERGE_TRACES";

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_WRITE_FINAL_TRACE:
        return L"FAILURE_WRITE_FINAL_TRACE";
    }

    return L"FAILURE_UNKNOWN_ERROR";
}

void PrintTraceStatistics(const TRACING_SESSION_STATISTICS& stats)
{
    std::wcout << L"Dropped MSVC events: " << stats.MSVCEventsLost << std::endl;
    std::wcout << L"Dropped MSVC buffers: " << stats.MSVCBuffersLost << std::endl;
    std::wcout << L"Dropped system events: " << stats.SystemEventsLost << std::endl;
    std::wcout << L"Dropped system buffers: " << stats.SystemBuffersLost << std::endl;
}

void PrintPrivacyNotice(const std::filesystem::path& outputFile)
{
    std::error_code ec;
    auto absolutePath = std::filesystem::absolute(outputFile, ec);

    std::wcout << "The trace ";
    
    if (!ec)
    {
        std::wcout << "\"" << absolutePath.c_str() << "\" ";
    }
    else
    {
        std::wcout << "\"" << outputFile.c_str() << "\" ";
    }

    std::wcout << L"may contain personally identifiable information. This includes, but is not limited to, "
        L"paths of files that were accessed and names of processes that were running during the collection. "
        L"Please be aware of this when sharing this trace with others." << std::endl;
}

void PrintError(BUILD_INSIGHTS_RESULT_CODE failureCode)
{
    switch (failureCode)
    {
    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_INSUFFICIENT_PRIVILEGES:
        std::wcout << "This operation requires administrator privileges.";
        break;

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_DROPPED_EVENTS:
        std::wcout << "Events were dropped during the trace. Please try recollecting the trace.";
        break;

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_UNSUPPORTED_OS:
        std::wcout << "The version of Microsoft Visual C++ Build Insights that vcperf is using "
            "does not support the version of the operating system that the trace was collected on. "
            "Please try updating vcperf to the latest version.";
        break;

    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_START_SYSTEM_TRACE:
    case BUILD_INSIGHTS_RESULT_CODE_FAILURE_START_MSVC_TRACE:
        std::wcout << "A trace that is currently being collected on your system is preventing vcperf "
            "from starting a new one. This can occur if you forgot to stop a vcperf trace prior to "
            "running the start command, or if processes other than vcperf have started ETW traces of "
            "their own. Please try running the vcperf /stop or /stopnoanalyze commands on your previously "
            "started traces. If you do not remember the session name that was used for starting a previous "
            "vcperf trace, or if you don't recall starting one at all, you can use the 'tracelog -l' command "
            "to list all ongoing tracing sessions on your system. Your currently ongoing vcperf trace will "
            "show up as MSVC_BUILD_INSIGHTS_SESSION_<session name that was passed to vcperf /start>. You can "
            "then issue a vcperf /stop or /stopnoanalyze command with the identified session name.";
        break;

    default:
        std::wcout << L"ERROR CODE: " << ResultCodeToString(failureCode);
    }

    std::wcout << std::endl;
}

HRESULT DoStart(const std::wstring& sessionName, bool cpuSampling, VerbosityLevel verbosityLevel)
{
    TRACING_SESSION_OPTIONS options{};

    options.SystemEventFlags |= BUILD_INSIGHTS_TRACINGSESSION_SYSTEM_EVENTS_CONTEXT;

    switch (verbosityLevel)
    {
    case VerbosityLevel::VERBOSE:
        options.MsvcEventFlags |= BUILD_INSIGHTS_TRACINGSESSION_MSVC_EVENTS_FRONTEND_TEMPLATE_INSTANTIATIONS;

    case VerbosityLevel::MEDIUM:
        options.MsvcEventFlags |= BUILD_INSIGHTS_TRACINGSESSION_MSVC_EVENTS_FRONTEND_FILES;
        options.MsvcEventFlags |= BUILD_INSIGHTS_TRACINGSESSION_MSVC_EVENTS_BACKEND_FUNCTIONS;

    case VerbosityLevel::LIGHT:
        options.MsvcEventFlags |= BUILD_INSIGHTS_TRACINGSESSION_MSVC_EVENTS_BASIC;
    }

    if (cpuSampling) {
        options.SystemEventFlags |= BUILD_INSIGHTS_TRACINGSESSION_SYSTEM_EVENTS_CPU_SAMPLES;
    }

    std::wcout << L"Starting tracing session " << sessionName << L"..." << std::endl;

    auto rc = StartTracingSession(sessionName.c_str(), options);

    if (rc != BUILD_INSIGHTS_RESULT_CODE_SUCCESS) 
    {
        std::wcout << "Failed to start trace." << std::endl;
        PrintError(rc);
        
        return E_FAIL;
    }

    std::wcout << L"Tracing session started successfully!" << std::endl;

    return S_OK;
}


HRESULT DoStop(const std::wstring& sessionName, const std::filesystem::path& outputFile, bool analyzeTemplates)
{
    TRACING_SESSION_STATISTICS statistics{};

    ExpensiveTemplateInstantiationCache etic{analyzeTemplates};
    ContextBuilder cb;
    MiscellaneousCache mc;
    BuildExplorerView bev{&cb, &mc};
    FunctionsView funcv{&cb, &mc};
    FilesView fv{&cb, &mc};
    TemplateInstantiationsView tiv{&cb, &etic, &mc, analyzeTemplates};

    auto analyzerGroup = MakeStaticAnalyzerGroup(&cb, &etic, &mc);
    auto reloggerGroup = MakeStaticReloggerGroup(&etic, &mc, &cb, &bev, &funcv, &fv, &tiv);

    RELOG_RETENTION_OPTIONS options{};

    options.RetainedMSVCEvents |= BUILD_INSIGHTS_RELOG_RETENTION_MSVC_INVOCATION_DATA;
    options.RetainedSystemEvents |= BUILD_INSIGHTS_RELOG_RETENTION_SYSTEM_CPU_SAMPLES;

    std::wcout << L"Stopping and analyzing tracing session " << sessionName << L"..." << std::endl;

    int analysisPassCount = analyzeTemplates ? 2 : 1;

    auto rc = StopAndRelogTracingSession(sessionName.c_str(), outputFile.c_str(),
        &statistics, analysisPassCount, options, analyzerGroup, reloggerGroup);

    PrintTraceStatistics(statistics);

    if (rc != BUILD_INSIGHTS_RESULT_CODE_SUCCESS)
    {
        std::wcout << "Failed to stop trace." << std::endl;
        PrintError(rc);

        return E_FAIL;
    }

    PrintPrivacyNotice(outputFile);
    std::wcout << L"Tracing session stopped successfully!" << std::endl;

    return S_OK;
}

HRESULT DoStopNoAnalyze(const std::wstring& sessionName, const std::filesystem::path& outputFile)
{
    TRACING_SESSION_STATISTICS statistics{};

    std::wcout << L"Stopping tracing session " << sessionName << L"..." << std::endl;

    auto rc = StopTracingSession(sessionName.c_str(), outputFile.c_str(), &statistics);

    PrintTraceStatistics(statistics);

    if (rc != BUILD_INSIGHTS_RESULT_CODE_SUCCESS)
    {
        std::wcout << "Failed to stop trace." << std::endl;
        PrintError(rc);

        return E_FAIL;
    }

    PrintPrivacyNotice(outputFile);
    std::wcout << L"Tracing session stopped successfully!" << std::endl;

    return S_OK;
}

HRESULT DoAnalyze(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile, bool analyzeTemplates)
{
    ExpensiveTemplateInstantiationCache etic{analyzeTemplates};
    ContextBuilder cb;
    MiscellaneousCache mc;
    BuildExplorerView bev{&cb, &mc};
    FunctionsView funcv{&cb, &mc};
    FilesView fv{&cb, &mc};
    TemplateInstantiationsView tiv{&cb, &etic, &mc, analyzeTemplates};

    auto analyzerGroup = MakeStaticAnalyzerGroup(&cb, &etic, &mc);
    auto reloggerGroup = MakeStaticReloggerGroup(&etic, &mc, &cb, &bev, &funcv, &fv, &tiv);

    RELOG_RETENTION_OPTIONS options{};

    options.RetainedMSVCEvents |= BUILD_INSIGHTS_RELOG_RETENTION_MSVC_INVOCATION_DATA;
    options.RetainedSystemEvents |= BUILD_INSIGHTS_RELOG_RETENTION_SYSTEM_CPU_SAMPLES;

    std::wcout << L"Analyzing..." << std::endl;

    int analysisPassCount = analyzeTemplates ? 2 : 1;

    auto rc = Relog(inputFile.c_str(), outputFile.c_str(), analysisPassCount, options, analyzerGroup, reloggerGroup);

    if (rc != BUILD_INSIGHTS_RESULT_CODE_SUCCESS)
    {
        std::wcout << "Failed to analyze trace." << std::endl;
        PrintError(rc);

        return E_FAIL;
    }

    PrintPrivacyNotice(outputFile);
    std::wcout << L"Analysis completed successfully!" << std::endl;

    return S_OK;
}
