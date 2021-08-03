#pragma once

#include <string>
#include <map>

#include "VcperfBuildInsights.h"


namespace vcperf
{

class StatisticsCollector : public BI::IAnalyzer {
public:
	StatisticsCollector();

	BI::AnalysisControl OnEndAnalysisPass() override;
	BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
	BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;

private:
	void OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup);
	void OnSymbolName(const SE::SymbolName& symbolName);
	void OnFileParse(const A::FrontEndFileGroup& files);
	void OnFunction(const A::Function& function);
	
	//global
	int passNumber_ = 0;

	//pass 0:
	std::vector<std::pair<uint64_t, std::string>> symbolNames_;
	std::vector<std::string> functionNamesDecorated_;
	std::vector<std::string> functionNamesUndecorated_;
	std::vector<std::string> parsedFilePaths_;

	struct Result {
		int count_ = 0;
		uint64_t totalTime_ = 0;	//CPU time, exclusive
		void operator+= (const Result& other);
	};

	//pass 1:
	std::vector<Result> parseTimes_;
	std::vector<Result> instantiationTimes_;
	std::vector<Result> generationTimes_;
};

}
