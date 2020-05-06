#pragma once

#include <filesystem>
#include <string>

namespace vcperf
{

enum class VerbosityLevel
{
    INVALID,
    LIGHT,
    MEDIUM,
    VERBOSE
};

HRESULT DoStart(const std::wstring& sessionName, bool cpuSampling, VerbosityLevel verbosityLevel);
HRESULT DoStop(const std::wstring& sessionName, const std::filesystem::path& outputFile, bool analyzeTemplates = false, bool generateChromeTrace = false);
HRESULT DoStopNoAnalyze(const std::wstring& sessionName, const std::filesystem::path& outputFile);
HRESULT DoAnalyze(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile, bool analyzeTemplates = false, bool generateChromeTrace = false);

} // namespace vcperf