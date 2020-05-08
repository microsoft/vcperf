#include <Windows.h>
#include "Commands.h"

#include <iostream>

#include "VcperfBuildInsights.h"

#include "Analyzers\ExpensiveTemplateInstantiationCache.h"
#include "Analyzers\ContextBuilder.h"
#include "Analyzers\MiscellaneousCache.h"
#include "Analyzers\ExecutionHierarchy.h"
#include "Views\BuildExplorerView.h"
#include "Views\FunctionsView.h"
#include "Views\FilesView.h"
#include "Views\TemplateInstantiationsView.h"
#include "Views\ChromeFlameGraph\ChromeFlameGraphView.h"

using namespace Microsoft::Cpp::BuildInsights;

namespace vcperf
{

const wchar_t* ResultCodeToString(RESULT_CODE rc)
{
    switch (rc)
    {
    case RESULT_CODE_SUCCESS:
        return L"SUCCESS";

    case RESULT_CODE_FAILURE_ANALYSIS_ERROR:
        return L"FAILURE_ANALYSIS_ERROR";

    case RESULT_CODE_FAILURE_CANCELLED:
        return L"FAILURE_CANCELLED";

    case RESULT_CODE_FAILURE_INVALID_INPUT_LOG_FILE:
        return L"FAILURE_INVALID_INPUT_LOG_FILE";

    case RESULT_CODE_FAILURE_INVALID_OUTPUT_LOG_FILE:
        return L"FAILURE_INVALID_OUTPUT_LOG_FILE";

    case RESULT_CODE_FAILURE_MISSING_ANALYSIS_CALLBACK:
        return L"FAILURE_MISSING_ANALYSIS_CALLBACK";

    case RESULT_CODE_FAILURE_MISSING_RELOG_CALLBACK:
        return L"FAILURE_MISSING_RELOG_CALLBACK";

    case RESULT_CODE_FAILURE_OPEN_INPUT_TRACE:
        return L"FAILURE_OPEN_INPUT_TRACE";

    case RESULT_CODE_FAILURE_PROCESS_TRACE:
        return L"FAILURE_PROCESS_TRACE";

    case RESULT_CODE_FAILURE_START_RELOGGER:
        return L"FAILURE_START_RELOGGER";

    case RESULT_CODE_FAILURE_DROPPED_EVENTS:
        return L"FAILURE_DROPPED_EVENTS";

    case RESULT_CODE_FAILURE_UNSUPPORTED_OS:
        return L"FAILURE_UNSUPPORTED_OS";

    case RESULT_CODE_FAILURE_INVALID_TRACING_SESSION_NAME:
        return L"FAILURE_INVALID_TRACING_SESSION_NAME";

    case RESULT_CODE_FAILURE_INSUFFICIENT_PRIVILEGES:
        return L"FAILURE_INSUFFICIENT_PRIVILEGES";

    case RESULT_CODE_FAILURE_GENERATE_GUID:
        return L"FAILURE_GENERATE_GUID";

    case RESULT_CODE_FAILURE_OBTAINING_TEMP_DIRECTORY:
        return L"FAILURE_OBTAINING_TEMP_DIRECTORY";

    case RESULT_CODE_FAILURE_CREATE_TEMPORARY_DIRECTORY:
        return L"FAILURE_CREATE_TEMPORARY_DIRECTORY";

    case RESULT_CODE_FAILURE_START_SYSTEM_TRACE:
        return L"FAILURE_START_SYSTEM_TRACE";

    case RESULT_CODE_FAILURE_START_MSVC_TRACE:
        return L"FAILURE_START_MSVC_TRACE";

    case RESULT_CODE_FAILURE_STOP_MSVC_TRACE:
        return L"FAILURE_STOP_MSVC_TRACE";

    case RESULT_CODE_FAILURE_STOP_SYSTEM_TRACE:
        return L"FAILURE_STOP_SYSTEM_TRACE";

    case RESULT_CODE_FAILURE_SESSION_DIRECTORY_RESOLUTION:
        return L"FAILURE_SESSION_DIRECTORY_RESOLUTION";

    case RESULT_CODE_FAILURE_MSVC_TRACE_FILE_NOT_FOUND:
        return L"FAILURE_MSVC_TRACE_FILE_NOT_FOUND";

    case RESULT_CODE_FAILURE_MERGE_TRACES:
        return L"FAILURE_MERGE_TRACES";
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

void PrintError(RESULT_CODE failureCode)
{
    switch (failureCode)
    {
    case RESULT_CODE_FAILURE_INSUFFICIENT_PRIVILEGES:
        std::wcout << "This operation requires administrator privileges.";
        break;

    case RESULT_CODE_FAILURE_DROPPED_EVENTS:
        std::wcout << "Events were dropped during the trace. Please try recollecting the trace.";
        break;

    case RESULT_CODE_FAILURE_UNSUPPORTED_OS:
        std::wcout << "The version of Microsoft Visual C++ Build Insights that vcperf is using "
            "does not support the version of the operating system that the trace was collected on. "
            "Please try updating vcperf to the latest version.";
        break;

    case RESULT_CODE_FAILURE_START_SYSTEM_TRACE:
    case RESULT_CODE_FAILURE_START_MSVC_TRACE:
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

RESULT_CODE StopToWPA(const std::wstring& sessionName, const std::filesystem::path& outputFile, bool analyzeTemplates,
    TRACING_SESSION_STATISTICS& statistics)
{
    ExpensiveTemplateInstantiationCache etic{ analyzeTemplates };
    ContextBuilder cb;
    MiscellaneousCache mc;
    BuildExplorerView bev{ &cb, &mc };
    FunctionsView funcv{ &cb, &mc };
    FilesView fv{ &cb, &mc };
    TemplateInstantiationsView tiv{ &cb, &etic, &mc, analyzeTemplates };

    auto analyzerGroup = MakeStaticAnalyzerGroup(&cb, &etic, &mc);
    auto reloggerGroup = MakeStaticReloggerGroup(&etic, &mc, &cb, &bev, &funcv, &fv, &tiv);

    unsigned long long systemEventsRetentionFlags = RELOG_RETENTION_SYSTEM_EVENT_FLAGS_CPU_SAMPLES;

    int analysisPassCount = analyzeTemplates ? 2 : 1;

    return StopAndRelogTracingSession(sessionName.c_str(), outputFile.c_str(),
        &statistics, analysisPassCount, systemEventsRetentionFlags, analyzerGroup, reloggerGroup);
}

RESULT_CODE StopToChromeTrace(const std::wstring& sessionName, const std::filesystem::path& outputFile, bool analyzeTemplates,
    TRACING_SESSION_STATISTICS& statistics)
{
    ExecutionHierarchy eh;
    ChromeFlameGraphView::Filter f{ analyzeTemplates,
                                    std::chrono::milliseconds(10),
                                    std::chrono::milliseconds(10) };
    ChromeFlameGraphView cfgv{ &eh, outputFile, f };

    auto analyzerGroup = MakeStaticAnalyzerGroup(&eh, &cfgv);
    int analysisPassCount = 1;

    return StopAndAnalyzeTracingSession(sessionName.c_str(), analysisPassCount, &statistics, analyzerGroup);
}

RESULT_CODE AnalyzeToWPA(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile, bool analyzeTemplates)
{
    ExpensiveTemplateInstantiationCache etic{ analyzeTemplates };
    ContextBuilder cb;
    MiscellaneousCache mc;
    BuildExplorerView bev{ &cb, &mc };
    FunctionsView funcv{ &cb, &mc };
    FilesView fv{ &cb, &mc };
    TemplateInstantiationsView tiv{ &cb, &etic, &mc, analyzeTemplates };

    auto analyzerGroup = MakeStaticAnalyzerGroup(&cb, &etic, &mc);
    auto reloggerGroup = MakeStaticReloggerGroup(&etic, &mc, &cb, &bev, &funcv, &fv, &tiv);

    unsigned long long systemEventsRetentionFlags = RELOG_RETENTION_SYSTEM_EVENT_FLAGS_CPU_SAMPLES;

    int analysisPassCount = analyzeTemplates ? 2 : 1;

    return Relog(inputFile.c_str(), outputFile.c_str(), analysisPassCount,
        systemEventsRetentionFlags, analyzerGroup, reloggerGroup);
}

RESULT_CODE AnalyzeToChromeTrace(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile, bool analyzeTemplates)
{
    ExecutionHierarchy eh;
    ChromeFlameGraphView::Filter f{ analyzeTemplates,
                                    std::chrono::milliseconds(10),
                                    std::chrono::milliseconds(10) };
    ChromeFlameGraphView cfgv{ &eh, outputFile, f };

    auto analyzerGroup = MakeStaticAnalyzerGroup(&eh, &cfgv);
    int analysisPassCount = 1;

    return Analyze(inputFile.c_str(), analysisPassCount, analyzerGroup);
}

HRESULT DoStart(const std::wstring& sessionName, bool cpuSampling, VerbosityLevel verbosityLevel)
{
    TRACING_SESSION_OPTIONS options{};

    options.SystemEventFlags |= TRACING_SESSION_SYSTEM_EVENT_FLAGS_CONTEXT;

    switch (verbosityLevel)
    {
    case VerbosityLevel::VERBOSE:
        options.MsvcEventFlags |= TRACING_SESSION_MSVC_EVENT_FLAGS_FRONTEND_TEMPLATE_INSTANTIATIONS;

    case VerbosityLevel::MEDIUM:
        options.MsvcEventFlags |= TRACING_SESSION_MSVC_EVENT_FLAGS_FRONTEND_FILES;
        options.MsvcEventFlags |= TRACING_SESSION_MSVC_EVENT_FLAGS_BACKEND_FUNCTIONS;

    case VerbosityLevel::LIGHT:
        options.MsvcEventFlags |= TRACING_SESSION_MSVC_EVENT_FLAGS_BASIC;
    }

    if (cpuSampling) {
        options.SystemEventFlags |= TRACING_SESSION_SYSTEM_EVENT_FLAGS_CPU_SAMPLES;
    }

    std::wcout << L"Starting tracing session " << sessionName << L"..." << std::endl;

    auto rc = StartTracingSession(sessionName.c_str(), options);

    if (rc != RESULT_CODE_SUCCESS) 
    {
        std::wcout << "Failed to start trace." << std::endl;
        PrintError(rc);
        
        return E_FAIL;
    }

    std::wcout << L"Tracing session started successfully!" << std::endl;

    return S_OK;
}


HRESULT DoStop(const std::wstring& sessionName, const std::filesystem::path& outputFile, bool analyzeTemplates, bool generateChromeTrace)
{
    std::wcout << L"Stopping and analyzing tracing session " << sessionName << L"..." << std::endl;

    TRACING_SESSION_STATISTICS statistics{};
    RESULT_CODE rc;
    if (!generateChromeTrace)
    {
        rc = StopToWPA(sessionName, outputFile, analyzeTemplates, statistics);
    }
    else
    {
        rc = StopToChromeTrace(sessionName, outputFile, analyzeTemplates, statistics);
    }

    PrintTraceStatistics(statistics);

    if (rc != RESULT_CODE_SUCCESS)
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

    if (rc != RESULT_CODE_SUCCESS)
    {
        std::wcout << "Failed to stop trace." << std::endl;
        PrintError(rc);

        return E_FAIL;
    }

    PrintPrivacyNotice(outputFile);
    std::wcout << L"Tracing session stopped successfully!" << std::endl;

    return S_OK;
}

HRESULT DoAnalyze(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile, bool analyzeTemplates, bool generateChromeTrace)
{
    std::wcout << L"Analyzing..." << std::endl;
    
    RESULT_CODE rc;
    if (!generateChromeTrace)
    {
        rc = AnalyzeToWPA(inputFile, outputFile, analyzeTemplates);
    }
    else
    {
        rc = AnalyzeToChromeTrace(inputFile, outputFile, analyzeTemplates);
    }

    if (rc != RESULT_CODE_SUCCESS)
    {
        std::wcout << "Failed to analyze trace." << std::endl;
        PrintError(rc);

        return E_FAIL;
    }

    PrintPrivacyNotice(outputFile);
    std::wcout << L"Analysis completed successfully!" << std::endl;

    return S_OK;
}

} // namespace vcperf
