#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <tuple>

#include "CppBuildInsights.hpp"

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

class ExpensiveTemplateInstantiationCache : public IAnalyzer
{
    struct PrimaryTemplateStats
    {
        std::vector<unsigned long long> PrimaryKeys;
        unsigned int TotalMicroseconds;
    };

public:

    ExpensiveTemplateInstantiationCache(bool isEnabled):
        analysisCount_{0},
        analysisPass_{0},
        traceDuration_{0},
        cachedSymbolNames_{},
        primaryTemplateStats_{},
        keysToConsider_{},
        localPrimaryTemplateTimes_{},
        isEnabled_{isEnabled}
    {}

    std::tuple<bool, const char*, const char*> 
        GetTemplateInstantiationInfo(const TemplateInstantiation& ti) const;

    AnalysisControl OnTraceInfo(const TraceInfo& traceInfo) override 
    {
        traceDuration_ = traceInfo.Duration();
        return AnalysisControl::CONTINUE;
    }

    AnalysisControl OnBeginAnalysis() override
    {
        analysisCount_++;
        return AnalysisControl::CONTINUE;
    }

    AnalysisControl OnEndAnalysis() override
    {
        analysisCount_--;
        return AnalysisControl::CONTINUE;
    }

    AnalysisControl OnBeginAnalysisPass() override 
    {
        analysisPass_++;
        return AnalysisControl::CONTINUE;
    }

    AnalysisControl OnStopActivity(const EventStack& eventStack) override;
    AnalysisControl OnSimpleEvent(const EventStack& eventStack) override;
    AnalysisControl OnEndAnalysisPass() override;

private:
    bool IsRelogging() const {
        return analysisCount_ == 1;
    }

    void OnTemplateInstantiation(const TemplateInstantiation& instantiation);
    void OnSymbolName(const SymbolName& symbol);
    void Phase1RegisterPrimaryTemplateLocalTime(const TemplateInstantiation& instantiation);
    void Phase1MergePrimaryTemplateDuration(const SymbolName& symbol);
    void Phase2RegisterSpecializationKey(const TemplateInstantiation& instantiation);
    void Phase2MergeSpecializationKey(const SymbolName& symbol);
    void DetermineTopPrimaryTemplates();

    int analysisCount_;
    int analysisPass_;

    std::chrono::nanoseconds traceDuration_;

    // All phases
    std::unordered_set<std::string> cachedSymbolNames_;
    std::unordered_map<unsigned long long, const char*> keysToConsider_;

    // Phase 1
    std::unordered_map<const char*, PrimaryTemplateStats> primaryTemplateStats_;
    std::unordered_map<unsigned long long, unsigned int> localPrimaryTemplateTimes_;

    // Phase 2
    std::unordered_set<unsigned long long> localSpecializationKeysToConsider_;

    bool isEnabled_;

};