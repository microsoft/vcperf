#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <cwctype>

#include "Commands.h"

#include "VcperfBuildInsights.h"

#ifndef VCPERF_VERSION
#define VCPERF_VERSION DEVELOPER VERSION
#endif

#define STRINGIFY(x) #x
#define GET_VERSION_STRING(ver) STRINGIFY(ver)

#define VCPERF_VERSION_STRING GET_VERSION_STRING(VCPERF_VERSION)

// TODO: Start using some kind of command line parsing library

enum class StartSubCommand
{
    NONE,
    NO_CPU_SAMPLING,
    LEVEL1,
    LEVEL2,
    LEVEL3
};

using namespace Microsoft::Cpp::BuildInsights;

using namespace vcperf;

bool CheckCommand(std::wstring arg, const wchar_t* value)
{
    auto ciCompare = [](wchar_t c1, wchar_t c2) { return std::towupper(c1) == std::towupper(c2); };

    std::wstring slash = std::wstring{ L"/" } + value;
    std::wstring hyphen = std::wstring{ L"-" } + value;

    return      std::equal(begin(arg), end(arg), begin(slash), end(slash), ciCompare)
            ||  std::equal(begin(arg), end(arg), begin(hyphen), end(hyphen), ciCompare);
}

bool ValidateFile(const std::filesystem::path& file, bool isInput, const std::wstring& extension)
{
    if (file.extension() != extension) {
        std::wcout << L"ERROR: Your " << (isInput ? L"input" : L"output") << L" file must have the " << extension << L" extension." << std::endl;
        return false;
    }

    if (isInput && !std::filesystem::exists(file)) {
        std::wcout << L"ERROR: File not found: " << file << std::endl;
        return false;
    }

    return true;
}

StartSubCommand CheckStartSubCommands(const wchar_t* arg)
{
    if (CheckCommand(arg, L"nocpusampling")) {
        return StartSubCommand::NO_CPU_SAMPLING;
    }

    if (CheckCommand(arg, L"level1")) {
        return StartSubCommand::LEVEL1;
    }

    if (CheckCommand(arg, L"level2")) {
        return StartSubCommand::LEVEL2;
    }

    if (CheckCommand(arg, L"level3")) {
        return StartSubCommand::LEVEL3;
    }

    return StartSubCommand::NONE;
}


int ParseStopOrAnalyze(int argc, wchar_t* argv[], const wchar_t* command, const wchar_t* sessionOrInputHelp,
    std::wstring& firstArg, std::wstring& outputFile, bool& analyzeTemplates, bool& generateChromeTrace)
{
    if (argc < 4)
    {
        std::wcout << L"vcperf.exe " << command << " [/templates] [/chrometrace] " << sessionOrInputHelp << " outputFile.etl" << std::endl;
        return E_FAIL;
    }

    int curArgc = 2;

    analyzeTemplates = false;
    generateChromeTrace = false;
    std::wstring arg = argv[curArgc++];
    firstArg = arg;

    if (CheckCommand(firstArg, L"templates"))
    {
        firstArg = argv[curArgc++];
        analyzeTemplates = true;
    }

    if (analyzeTemplates && argc < 5)
    {
        std::wcout << L"vcperf.exe " << command << " [/templates] [/chrometrace] " << sessionOrInputHelp << " outputFile.etl" << std::endl;
        return E_FAIL;
    }

    if (CheckCommand(firstArg, L"chrometrace"))
    {
        firstArg = argv[curArgc++];
        generateChromeTrace = true;
    }

    if (generateChromeTrace && argc < 5)
    {
        std::wcout << L"vcperf.exe " << command << " [/templates] [/chrometrace] " << sessionOrInputHelp << " outputFile.etl" << std::endl;
        return E_FAIL;
    }

    if (analyzeTemplates && generateChromeTrace && argc < 6)
    {
        std::wcout << L"vcperf.exe " << command << " [/templates] [/chrometrace] " << sessionOrInputHelp << " outputFile.etl" << std::endl;
        return E_FAIL;
    }

    outputFile = argv[curArgc++];

    if (!ValidateFile(outputFile, false, generateChromeTrace ? L".json" : L".etl")) {
        return E_FAIL;
    }

    return S_OK;
}

int wmain(int argc, wchar_t* argv[])
{
    std::wcout << L"Microsoft (R) Visual C++ (R) Performance Analyzer " << 
        VCPERF_VERSION_STRING << std::endl;

    HRESULT hr = CoInitialize(nullptr);

    if (hr != S_OK) {
        std::wcout << L"ERROR: failed to initialize COM." << std::endl;
    }

    if (argc < 2) 
    {
        std::wcout << std::endl;
        std::wcout << L"USAGE:" << std::endl;
        std::wcout << L"vcperf.exe /start [/nocpusampling] [/level1 | /level2 | /level3] sessionName" << std::endl;
        std::wcout << L"vcperf.exe /stop [/templates] [/chrometrace] sessionName outputFile.etl" << std::endl;
        std::wcout << L"vcperf.exe /stopnoanalyze sessionName outputRawFile.etl" << std::endl;
        std::wcout << L"vcperf.exe /analyze [/templates] [/chrometrace] inputRawFile.etl output.etl" << std::endl;

        std::wcout << std::endl;

        return E_FAIL;
    }

    if (CheckCommand(argv[1], L"start")) 
    {
        if (argc < 3) 
        {
            std::wcout << L"vcperf.exe /start [/nocpusampling] [/level1 | /level2 | /level3] sessionName" << std::endl;
            return E_FAIL;
        }

        bool noCpuSamplingSpecified = false;
        VerbosityLevel verbosityLevel = VerbosityLevel::INVALID;

        int currentArg = 2;
        StartSubCommand subCommand = CheckStartSubCommands(argv[currentArg]);

        while (subCommand != StartSubCommand::NONE)
        {
            if (subCommand == StartSubCommand::NO_CPU_SAMPLING)
            {
                if (noCpuSamplingSpecified)
                {
                    std::wcout << L"ERROR: you can only specify /nocpusampling once." << std::endl;
                    std::wcout << L"vcperf.exe /start [/nocpusampling] [/level1 | /level2 | /level3] sessionName" << std::endl;
                    return E_FAIL;
                }
                noCpuSamplingSpecified = true;
            }
            else if (verbosityLevel != VerbosityLevel::INVALID)
            {
                std::wcout << L"ERROR: you can only specify one verbosity level: /level1, /level2, or /level3." << std::endl;
                std::wcout << L"vcperf.exe /start [/nocpusampling] [/level1 | /level2 | /level3] sessionName" << std::endl;
                return E_FAIL;
            }
            else
            {
                switch (subCommand)
                {
                case StartSubCommand::LEVEL1:
                    verbosityLevel = VerbosityLevel::LIGHT;
                    break;

                case StartSubCommand::LEVEL2:
                    verbosityLevel = VerbosityLevel::MEDIUM;
                    break;

                case StartSubCommand::LEVEL3:
                    verbosityLevel = VerbosityLevel::VERBOSE;
                    break;
                }
            }

            currentArg++;

            if (currentArg >= argc) {
                break;
            }

            subCommand = CheckStartSubCommands(argv[currentArg]);
        }

        if (currentArg >= argc) 
        {
            std::wcout << L"ERROR: a session name must be specified." << std::endl;
            std::wcout << L"vcperf.exe /start [/nocpusampling] [/level1 | /level2 | /level3] sessionName" << std::endl;
            return E_FAIL;
        }

        if (verbosityLevel == VerbosityLevel::INVALID)
        {
            verbosityLevel = VerbosityLevel::MEDIUM;
        }

        std::wstring sessionName = argv[currentArg];

        return DoStart(sessionName, !noCpuSamplingSpecified, verbosityLevel);
    }
    else if (CheckCommand(argv[1], L"stop")) 
    {
        std::wstring sessionName, outputFile;
        bool analyzeTemplates, generateChromeTrace;

        if (S_OK != ParseStopOrAnalyze(argc, argv, L"/stop", L"sessionName", sessionName, outputFile, analyzeTemplates, generateChromeTrace)) {
            return E_FAIL;
        }

        return DoStop(sessionName, outputFile, analyzeTemplates, generateChromeTrace);
    }
    else if (CheckCommand(argv[1], L"stopnoanalyze")) 
    {
        if (argc < 4) 
        {
            std::wcout << L"vcperf.exe /stopnoanalyze sessionName outputRawFile.etl" << std::endl;
            return E_FAIL;
        }

        std::wstring sessionName = argv[2];
        std::filesystem::path outputFile = argv[3];

        if (!ValidateFile(outputFile, false, L".etl")) {
            return E_FAIL;
        }

        return DoStopNoAnalyze(sessionName, outputFile);
    }
    else if (CheckCommand(argv[1], L"analyze")) 
    {
        std::wstring inputFile, outputFile;
        bool analyzeTemplates, generateChromeTrace;

        if (S_OK != ParseStopOrAnalyze(argc, argv, L"/analyze", L"input.etl", inputFile, outputFile, analyzeTemplates, generateChromeTrace)) {
            return E_FAIL;
        }

        if (!ValidateFile(inputFile, true, L".etl")) {
            return E_FAIL;
        }

        return DoAnalyze(inputFile, outputFile, analyzeTemplates, generateChromeTrace);
    }
    else 
    {
        std::wcout << L"ERROR: Unknown command " << argv[1] << std::endl;

        return E_FAIL;
    }

    return S_OK;
}