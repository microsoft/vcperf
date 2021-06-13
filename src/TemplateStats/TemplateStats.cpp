#include "TemplateStats.h"
#include <algorithm>
#include <stdio.h>

using namespace Microsoft::Cpp::BuildInsights;
using namespace vcperf;


TemplateStatsAnalyzer::TemplateStatsAnalyzer(const std::string& prefix)
{
    prefix_ = prefix;
}

BI::AnalysisControl TemplateStatsAnalyzer::OnEndAnalysisPass()
{
    if (passNumber_ == 0) {
        std::sort(filteredKeys_.begin(), filteredKeys_.end());
    }
    else if (passNumber_ == 1) {
        filteredKeys_.clear();

        printf("Total time for template instantiations matching \"%s\":\n", prefix_.c_str());
        printf("  CPU Time:     %10.6lf / %10.6lf\n", 1e-9 * exclusiveCpuTime_, 1e-9 * inclusiveCpuTime_);
        printf("  Duration:     %10.6lf / %10.6lf\n", 1e-9 * exclusiveDuration_, 1e-9 * inclusiveDuration_);
    }
    passNumber_++;

    return AnalysisControl::CONTINUE;
}

AnalysisControl TemplateStatsAnalyzer::OnStopActivity(const EventStack& eventStack)
{
    if (passNumber_ == 1) {
        MatchEventStackInMemberFunction(eventStack, this, &TemplateStatsAnalyzer::OnFinishTemplateInstantiation);
    }

    return AnalysisControl::CONTINUE;
}

AnalysisControl TemplateStatsAnalyzer::OnSimpleEvent(const EventStack& eventStack)
{
    if (passNumber_ == 0) {
        MatchEventStackInMemberFunction(eventStack, this, &TemplateStatsAnalyzer::OnSymbolName);
    }

    return AnalysisControl::CONTINUE;
}

void TemplateStatsAnalyzer::OnSymbolName(const SE::SymbolName& symbolName)
{
    const char *name = symbolName.Name();
    size_t len = strlen(name);
    if (len >= prefix_.length() && memcmp(name, prefix_.data(), prefix_.length()) == 0) {
        filteredKeys_.push_back(symbolName.Key());
    }
}

bool TemplateStatsAnalyzer::MatchesPrefix(uint64_t primaryKey) const
{
    size_t pos = std::lower_bound(filteredKeys_.begin(), filteredKeys_.end(), primaryKey) - filteredKeys_.begin();
    return (pos < filteredKeys_.size() && filteredKeys_[pos] == primaryKey);
}

void TemplateStatsAnalyzer::OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup)
{
    bool matchLast = MatchesPrefix(templateInstantiationGroup.Back().PrimaryTemplateSymbolKey());
    bool matchAny = matchLast;
    for (int i = templateInstantiationGroup.Size() - 2; i >= 0 && !matchAny; i--)
        if (MatchesPrefix(templateInstantiationGroup[i].PrimaryTemplateSymbolKey()))
            matchAny = true;

    if (matchLast) {
        exclusiveCpuTime_ += templateInstantiationGroup.Back().ExclusiveCPUTime().count();
        exclusiveDuration_ += templateInstantiationGroup.Back().Duration().count();
    }
    if (matchAny) {
        inclusiveCpuTime_ += templateInstantiationGroup.Back().ExclusiveCPUTime().count();
        inclusiveDuration_ += templateInstantiationGroup.Back().Duration().count();
    }
}
