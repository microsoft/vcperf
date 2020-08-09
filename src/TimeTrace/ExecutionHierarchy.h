#pragma once

#include <unordered_map>
#include <vector>
#include <chrono>

#include "VcperfBuildInsights.h"

namespace vcperf
{

class ExecutionHierarchy : public BI::IAnalyzer
{
public:

    // controls which activities get ignored
    struct Filter
    {
        bool AnalyzeTemplates = false;
        std::chrono::milliseconds IgnoreTemplateInstantiationUnderMs = std::chrono::milliseconds(0);
        std::chrono::milliseconds IgnoreFunctionUnderMs = std::chrono::milliseconds(0);
    };

    struct Entry
    {
        unsigned long long Id = 0L;
        unsigned long ProcessId = 0L;
        unsigned long ThreadId = 0L;
        std::chrono::nanoseconds StartTimestamp = std::chrono::nanoseconds(0);
        std::chrono::nanoseconds StopTimestamp = std::chrono::nanoseconds(0);
        std::string Name;

        std::vector<Entry*> Children;
        std::unordered_map<std::string, std::string> Properties;

        bool OverlapsWith(const Entry* other) const;
    };

    typedef std::vector<const Entry*> TRoots;

public:

    ExecutionHierarchy(const Filter& filter);

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;

    const Entry* GetEntry(unsigned long long id) const;
    inline const TRoots& GetRoots() const { return roots_; }

private:

    void OnRootActivity(const A::Activity& root);
    void OnNestedActivity(const A::Activity& parent, const A::Activity& child);
    void OnFinishActivity(const A::Activity& activity);

    Entry* CreateEntry(const A::Activity& activity);

    void OnInvocation(const A::Invocation& invocation);
    void OnFrontEndFile(const A::FrontEndFile& frontEndFile);
    void OnThread(const A::Activity& parent, const A::Thread& thread);

    void OnFinishInvocation(const A::Invocation& invocation);
    void OnFinishFunction(const A::Activity& parent, const A::Function& function);
    void OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup);

    void OnSymbolName(const SE::SymbolName& symbolName);
    void OnCommandLine(const A::Activity& parent, const SE::CommandLine& commandLine);
    void OnEnvironmentVariable(const A::Activity& parent, const SE::EnvironmentVariable& environmentVariable);
    void OnFileInput(const A::Invocation& parent, const SE::FileInput& fileInput);
    void OnFileOutput(const A::Invocation& parent, const SE::FileOutput& fileOutput);

    void IgnoreEntry(unsigned long long id, unsigned long long parentId);
    void IgnoreEntry(unsigned long long id);

    std::unordered_map<unsigned long long, Entry> entries_;
    TRoots roots_;
    Filter filter_;

    typedef std::vector<std::string> TFileInputs;
    typedef std::vector<std::string> TFileOutputs;
    typedef std::pair<TFileInputs, TFileOutputs> TFileInputsOutputs;
    std::unordered_map<unsigned long long, TFileInputsOutputs> fileInputsOutputsPerInvocation_;

    typedef unsigned long long TSymbolKey;
    std::unordered_map<TSymbolKey, std::string> symbolNames_;
    typedef std::vector<unsigned long long> TUnresolvedTemplateInstantiationNames;
    std::unordered_map<TSymbolKey, TUnresolvedTemplateInstantiationNames> unresolvedTemplateInstantiationsPerSymbol_;
};

} // namespace vcperf
