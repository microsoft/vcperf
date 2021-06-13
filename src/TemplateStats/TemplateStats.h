#pragma once

#include <string>

#include "VcperfBuildInsights.h"


namespace vcperf
{

class TemplateStatsAnalyzer : public BI::IAnalyzer {
public:
	TemplateStatsAnalyzer(const std::string& wildcard);

	BI::AnalysisControl OnEndAnalysisPass() override;
	BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
	BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;

private:
	void OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup);
	void OnSymbolName(const SE::SymbolName& symbolName);

	bool MatchesPrefix(uint64_t primaryKey) const;

	int passNumber_ = 0;
	std::string wildcard_;

	//pass 0:
	std::vector<uint64_t> filteredKeys_;

	//pass 1:
	uint64_t exclusiveCpuTime_ = 0;
	uint64_t exclusiveDuration_ = 0;
	uint64_t inclusiveCpuTime_ = 0;
	uint64_t inclusiveDuration_ = 0;
};

}
