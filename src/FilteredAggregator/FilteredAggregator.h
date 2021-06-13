#pragma once

#include <string>

#include "VcperfBuildInsights.h"


namespace vcperf
{

class FilteringAggregator : public BI::IAnalyzer {
public:
	FilteringAggregator(const std::string& wildcard);

	BI::AnalysisControl OnEndAnalysisPass() override;
	BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
	BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;

private:
	void OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup);
	void OnSymbolName(const SE::SymbolName& symbolName);
	void OnFileParse(const A::FrontEndFileGroup& files);

	bool InstantiationMatchesWildcard(const A::TemplateInstantiation& templateInstantiation) const;
	bool FileParseMatchesWildcard(const A::FrontEndFile& file) const;

	struct WildcardTime {
		uint64_t exclusiveCpuTime_ = 0;
		uint64_t exclusiveDuration_ = 0;
		uint64_t subtractedCpuTime_ = 0;
		uint64_t subtractedDuration_ = 0;
		uint64_t inclusiveCpuTime_ = 0;
		uint64_t inclusiveDuration_ = 0;

		void Print() const;
	};
	template<class Activity> void UpdateWildcardTime(const BI::EventGroup<Activity>& activityGroup, bool (FilteringAggregator::*matchFunc)(const Activity&) const, WildcardTime& totalTime) const;

	//global
	int passNumber_ = 0;
	std::string wildcard_;

	//pass 0:
	std::vector<uint64_t> filteredKeys_;

	//pass 1:
	WildcardTime _instantiationsTime;
	WildcardTime _parsingTime;
};

}
