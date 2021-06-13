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

void TemplateStatsAnalyzer::WildcardTime::Print() const
{
    printf("  CPU Time:     %10.6lf / %10.6lf / %10.6lf\n", 1e-9 * exclusiveCpuTime_, 1e-9 * subtractedCpuTime_, 1e-9 * inclusiveCpuTime_);
    printf("  Duration:     %10.6lf / %10.6lf / %10.6lf\n", 1e-9 * exclusiveDuration_, 1e-9 * subtractedDuration_, 1e-9 * inclusiveDuration_);
}

BI::AnalysisControl TemplateStatsAnalyzer::OnEndAnalysisPass()
{
    if (passNumber_ == 0) {
        std::sort(filteredKeys_.begin(), filteredKeys_.end());
    }
    else if (passNumber_ == 1) {
        filteredKeys_.clear();

        printf("Total time for parsing files matching \"%s\":\n", wildcard_.c_str());
        _parsingTime.Print();
        printf("Total time for template instantiations matching \"%s\":\n", wildcard_.c_str());
        _instantiationsTime.Print();
    }
    passNumber_++;

    return AnalysisControl::CONTINUE;
}

AnalysisControl TemplateStatsAnalyzer::OnStopActivity(const EventStack& eventStack)
{
    if (passNumber_ == 1) {
        MatchEventStackInMemberFunction(eventStack, this, &TemplateStatsAnalyzer::OnFinishTemplateInstantiation);
        MatchEventStackInMemberFunction(eventStack, this, &TemplateStatsAnalyzer::OnFileParse);
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
}

bool TemplateStatsAnalyzer::InstantiationMatchesWildcard(const A::TemplateInstantiation& templateInstantiation) const
{
    uint64_t primaryKey = templateInstantiation.PrimaryTemplateSymbolKey();
    size_t pos = std::lower_bound(filteredKeys_.begin(), filteredKeys_.end(), primaryKey) - filteredKeys_.begin();
    return (pos < filteredKeys_.size() && filteredKeys_[pos] == primaryKey);
}

bool TemplateStatsAnalyzer::FileParseMatchesWildcard(const A::FrontEndFile& file) const
{
    const char *path = file.Path();
    //bool isHeader = !(WildcardMatch("*.cpp", path) || WildcardMatch("*.cxx", path) || WildcardMatch("*.c", path));
    return WildcardMatch(wildcard_.c_str(), path);
}

template<class Activity> void TemplateStatsAnalyzer::UpdateWildcardTime(const BI::EventGroup<Activity>& activityGroup, bool (TemplateStatsAnalyzer::*matchFunc)(const Activity&) const, WildcardTime& totalTime) const
{
    int n = (int)activityGroup.Size();
    uint64_t inclCpuTime = activityGroup.Back().CPUTime().count();
    uint64_t inclDuration = activityGroup.Back().Duration().count();
    uint64_t exclCpuTime = activityGroup.Back().ExclusiveCPUTime().count();
    uint64_t exclDuration = activityGroup.Back().ExclusiveDuration().count();

    if (n >= 1 && (this->*matchFunc)(activityGroup[n-1])) {
        bool hasMatchingAncestor = false;
        for (int i = n-2; i >= 0; i--)
            if ((this->*matchFunc)(activityGroup[i])) {
                hasMatchingAncestor = true;
                break;
            }
        if (!hasMatchingAncestor) {
            //instantiation stack looks like:  no, no, no, ..., no, match
            totalTime.inclusiveCpuTime_ += inclCpuTime;
            totalTime.inclusiveDuration_ += inclDuration;
        }
        //instantiation stack looks like:  any, any, any, ..., any, match
        totalTime.subtractedCpuTime_ += inclCpuTime;
        totalTime.subtractedDuration_ += inclDuration;
        totalTime.exclusiveCpuTime_ += exclCpuTime;
        totalTime.exclusiveDuration_ +=  exclDuration;
    }
    if (n >= 2 && (this->*matchFunc)(activityGroup[n-2])) {
        //instantiation stack looks like:  any any ... any match any
        totalTime.subtractedCpuTime_ -= inclCpuTime;
        totalTime.subtractedDuration_ -= inclDuration;
    }
}

void TemplateStatsAnalyzer::OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup)
{
    UpdateWildcardTime(templateInstantiationGroup, &TemplateStatsAnalyzer::InstantiationMatchesWildcard, _instantiationsTime);
}

void TemplateStatsAnalyzer::OnFileParse(const A::FrontEndFileGroup& files) {
    UpdateWildcardTime(files, &TemplateStatsAnalyzer::FileParseMatchesWildcard, _parsingTime);
}
