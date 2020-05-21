#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <tuple>

#include "VcperfBuildInsights.h"

namespace vcperf
{

class ExpensiveTemplateInstantiationCache : public BI::IAnalyzer
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
        GetTemplateInstantiationInfo(const A::TemplateInstantiation& ti) const;

    BI::AnalysisControl OnTraceInfo(const BI::TraceInfo& traceInfo) override 
    {
        traceDuration_ = traceInfo.Duration();
        return BI::AnalysisControl::CONTINUE;
    }

    BI::AnalysisControl OnBeginAnalysis() override
    {
        analysisCount_++;
        return BI::AnalysisControl::CONTINUE;
    }

    BI::AnalysisControl OnEndAnalysis() override
    {
        analysisCount_--;
        return BI::AnalysisControl::CONTINUE;
    }

    BI::AnalysisControl OnBeginAnalysisPass() override 
    {
        analysisPass_++;
        return BI::AnalysisControl::CONTINUE;
    }

    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnEndAnalysisPass() override;

private:
    bool IsRelogging() const {
        return analysisCount_ == 1;
    }

    void OnTemplateInstantiation(const A::TemplateInstantiation& instantiation);
    void OnSymbolName(const SE::SymbolName& symbol);
    void Phase1RegisterPrimaryTemplateLocalTime(const A::TemplateInstantiation& instantiation);
    void Phase1MergePrimaryTemplateDuration(const SE::SymbolName& symbol);
    void Phase2RegisterSpecializationKey(const A::TemplateInstantiation& instantiation);
    void Phase2MergeSpecializationKey(const SE::SymbolName& symbol);
    void DetermineTopPrimaryTemplates();

    int analysisCount_;
    int analysisPass_;

    std::chrono::nanoseconds traceDuration_;

    // All phases
    std::unordered_set<std::string> cachedSymbolNames_;
    std::unordered_map<unsigned long long, const char*> keysToConsider_;

    // Phase 1
    // The const char* keys in primaryTemplateStats_ point to the unique 
    // cached values in cachedSymbolNames_.
    std::unordered_map<const char*, PrimaryTemplateStats> primaryTemplateStats_;
    std::unordered_map<unsigned long long, unsigned int> localPrimaryTemplateTimes_;

    // Phase 2
    std::unordered_set<unsigned long long> localSpecializationKeysToConsider_;

    bool isEnabled_;

};

} // namespace vcperf