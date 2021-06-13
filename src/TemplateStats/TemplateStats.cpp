#include "TemplateStats.h"
#include <algorithm>
#include <stdio.h>
#include "Wildcard.h"

using namespace Microsoft::Cpp::BuildInsights;
using namespace vcperf;


TemplateStatsAnalyzer::TemplateStatsAnalyzer(const std::string& wildcard)
{
    wildcard_ = wildcard;
}

BI::AnalysisControl TemplateStatsAnalyzer::OnEndAnalysisPass()
{
    if (passNumber_ == 0) {
        std::sort(filteredKeys_.begin(), filteredKeys_.end());
    }
    else if (passNumber_ == 1) {
        filteredKeys_.clear();

        printf("Total time for template instantiations matching \"%s\":\n", wildcard_.c_str());
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
    if (WildcardMatch(wildcard_.c_str(), name)) {
        filteredKeys_.push_back(symbolName.Key());
    }
    /*if (len >= prefix_.length() && memcmp(name, prefix_.data(), prefix_.length()) == 0) {
        filteredKeys_.push_back(symbolName.Key());
    }*/
}

bool TemplateStatsAnalyzer::MatchesPrefix(uint64_t primaryKey) const
{
    size_t pos = std::lower_bound(filteredKeys_.begin(), filteredKeys_.end(), primaryKey) - filteredKeys_.begin();
    return (pos < filteredKeys_.size() && filteredKeys_[pos] == primaryKey);
}

void TemplateStatsAnalyzer::OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup)
{
    int n = (int)templateInstantiationGroup.Size();
    uint64_t inclCpuTime = templateInstantiationGroup.Back().CPUTime().count();
    uint64_t inclDuration = templateInstantiationGroup.Back().Duration().count();

    if (n >= 1 && MatchesPrefix(templateInstantiationGroup[n-1].PrimaryTemplateSymbolKey())) {
        bool hasMatchingAncestor = false;
        for (int i = n-2; i >= 0; i--)
            if (MatchesPrefix(templateInstantiationGroup[i].PrimaryTemplateSymbolKey())) {
                hasMatchingAncestor = true;
                break;
            }
        if (!hasMatchingAncestor) {
            //instantiation stack looks like:  no, no, no, ..., no, match
            inclusiveCpuTime_ += inclCpuTime;
            inclusiveDuration_ += inclDuration;
        }
        //instantiation stack looks like:  any, any, any, ..., any, match
        exclusiveCpuTime_ += inclCpuTime;
        exclusiveDuration_ += inclDuration;
    }
    if (n >= 2 && MatchesPrefix(templateInstantiationGroup[n-2].PrimaryTemplateSymbolKey())) {
        //instantiation stack looks like:  any any ... any match any
        exclusiveCpuTime_ -= inclCpuTime;
        exclusiveDuration_ -= inclDuration;
    }
}
