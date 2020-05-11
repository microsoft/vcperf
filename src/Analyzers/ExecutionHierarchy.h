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

    struct Entry
    {
        unsigned long long Id;
        unsigned long ProcessId;
        unsigned long ThreadId;
        std::chrono::nanoseconds StartTimestamp;
        std::chrono::nanoseconds StopTimestamp;
        std::string Name;

        std::vector<Entry*> Children;
        std::unordered_map<std::string, std::string> Properties;

        bool OverlapsWith(const Entry* other) const;
    };

    typedef std::vector<const Entry*> TRoots;

public:

    ExecutionHierarchy();

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
    void OnFunction(const A::Function& function);
    void OnTemplateInstantiation(const A::TemplateInstantiation& templateInstantiation);

    void OnSymbolName(const SE::SymbolName& symbolName);
    void OnCommandLine(const A::Activity& parent, const SE::CommandLine& commandLine);
    void OnEnvironmentVariable(const A::Activity& parent, const SE::EnvironmentVariable& environmentVariable);

    std::unordered_map<unsigned long long, Entry> entries_;
    TRoots roots_;

    typedef unsigned long long TSymbolKey;
    std::unordered_map<TSymbolKey, std::string> symbolNames_;
    typedef std::vector<unsigned long long> TUnresolvedTemplateInstantiationNames;
    std::unordered_map<TSymbolKey, TUnresolvedTemplateInstantiationNames> unresolvedTemplateInstantiationsPerSymbol_;
};

} // namespace vcperf
