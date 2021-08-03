#include "StatisticsCollector.h"
#include <algorithm>
#include <stdio.h>
#include "Undecorate.h"
#include <assert.h>

using namespace Microsoft::Cpp::BuildInsights;
using namespace vcperf;


void StatisticsCollector::Result::operator+= (const Result& other) {
    count_ += other.count_;
    totalTime_ += other.totalTime_;
}

StatisticsCollector::StatisticsCollector()
{
}

BI::AnalysisControl StatisticsCollector::OnEndAnalysisPass()
{
    if (passNumber_ == 0) {
        std::sort(parsedFilePaths_.begin(), parsedFilePaths_.end());
        parsedFilePaths_.resize(std::unique(parsedFilePaths_.begin(), parsedFilePaths_.end()) - parsedFilePaths_.begin());

        std::sort(symbolNames_.begin(), symbolNames_.end());

        std::sort(functionNamesDecorated_.begin(), functionNamesDecorated_.end());
        functionNamesDecorated_.resize(std::unique(functionNamesDecorated_.begin(), functionNamesDecorated_.end()) - functionNamesDecorated_.begin());
        Undecorate(functionNamesDecorated_, functionNamesUndecorated_);

        parseTimes_.resize(parsedFilePaths_.size());
        instantiationTimes_.resize(symbolNames_.size());
        generationTimes_.resize(functionNamesUndecorated_.size());
    }
    else if (passNumber_ == 1) {

        auto BuildTree = [](const std::vector<std::string>& paths, const std::vector<Result>& times, std::string separator, std::map<std::string, Result>& tree) {
            for (int i = 0; i < paths.size(); i++) {
                const std::string& text = paths[i];

                size_t pos = 0;
                while (1) {
                    tree["#" + text.substr(0, pos)] += times[i];
                    if (pos == text.size())
                        break;

                    pos = text.find(separator, pos+1);
                    if (pos == std::string::npos)
                        pos = text.size();
                }
            }
        };

        auto PrintTree = [](const std::map<std::string, Result>& tree, uint64_t totalTime) {
            double invTotal = 1.0 / totalTime;

            uint64_t totalTreeTime = tree.at("#").totalTime_;
            printf("  Total: %10.3lf  (%6.3lf%%)\n", totalTreeTime * 1e-9, totalTreeTime * invTotal * 100.0);

            struct PrintLine {
                std::string path;
                double totalTimeSec;
                int count;
                double totalTimePercent;
            };

            std::vector<PrintLine> arr;
            for (auto& kv : tree) {
                PrintLine pl;
                pl.path = kv.first;
                pl.totalTimeSec = kv.second.totalTime_ * 1e-9;
                pl.count = kv.second.count_;
                pl.totalTimePercent = kv.second.totalTime_ * invTotal * 100.0;
                if (pl.path.size() > 120)
                    pl.path = pl.path.substr(0, 117) + "...";
                arr.push_back(pl);
            }

            static const double MIN_TIME_SEC = 1e-3;
            static const double MIN_TIME_PERCENT = 1e-2;

            printf("\n  sorted by total time:\n");
            std::sort(arr.begin(), arr.end(), [](const PrintLine& a, const PrintLine& b) {
                return a.totalTimeSec > b.totalTimeSec;
            });
            for (int i = 0; i < arr.size(); i++) {
                if (arr[i].totalTimeSec < MIN_TIME_SEC || arr[i].totalTimePercent < MIN_TIME_PERCENT)
                    break;
                printf("    %10.3lf | %6d  (%6.3lf%%)  :  %s\n", arr[i].totalTimeSec, arr[i].count, arr[i].totalTimePercent, arr[i].path.c_str());
            }

            printf("\n  sorted lexicographically:\n");
            std::sort(arr.begin(), arr.end(), [](const PrintLine& a, const PrintLine& b) {
                return a.path < b.path;
            });
            for (int i = 0; i < arr.size(); i++) {
                if (arr[i].totalTimeSec < MIN_TIME_SEC || arr[i].totalTimePercent < MIN_TIME_PERCENT)
                    continue;
                printf("    %10.3lf | %6d  (%6.3lf%%)  :  %s\n", arr[i].totalTimeSec, arr[i].count, arr[i].totalTimePercent, arr[i].path.c_str());
            }
        };

        auto FilterTemplateArguments = [](std::string path) -> std::string {
            int totalBalance = 0;
            int lastBalanceZero = 0;
            for (int i = 0; i < path.size(); i++) {
                if (path[i] == '<')
                    totalBalance++;
                if (path[i] == '>')
                    totalBalance--;
                if (totalBalance == 0)
                    lastBalanceZero = i + 1;
            }

            std::string res;
            int runningBalance = 0;
            for (int i = 0; i < path.size(); i++) {
                if (path[i] == '<')
                    runningBalance++;
                if (runningBalance == 0 || i >= lastBalanceZero)
                    res += path[i];
                if (path[i] == '>')
                    runningBalance--;
            }
            return res;
        };

        std::vector<std::string> generationPaths;
        for (int i = 0; i < functionNamesUndecorated_.size(); i++)
            generationPaths.push_back(FilterTemplateArguments(functionNamesUndecorated_[i]));

        std::vector<std::string> symbolStrNames(symbolNames_.size());
        for (int i = 0; i < symbolNames_.size(); i++)
            symbolStrNames[i] = FilterTemplateArguments(symbolNames_[i].second);

        std::map<std::string, Result> parseTree_;
        std::map<std::string, Result> instantiationTree_;
        std::map<std::string, Result> generationTree_;
        BuildTree(parsedFilePaths_, parseTimes_, "\\", parseTree_);
        BuildTree(symbolStrNames, instantiationTimes_, "::", instantiationTree_);
        BuildTree(generationPaths, generationTimes_, "::", generationTree_);

        uint64_t totalTime = parseTree_.at("#").totalTime_ + instantiationTree_.at("#").totalTime_ + generationTree_.at("#").totalTime_;
        printf("All reported values are exclusive CPU times.\n");

        printf("\n\n\nFile parsing:\n");
        PrintTree(parseTree_, totalTime);
        printf("\n\n\nTemplate instantiation:\n");
        PrintTree(instantiationTree_, totalTime);
        printf("\n\n\nCode generation:\n");
        PrintTree(generationTree_, totalTime);
    }
    passNumber_++;

    return AnalysisControl::CONTINUE;
}

AnalysisControl StatisticsCollector::OnStopActivity(const EventStack& eventStack)
{
    if (passNumber_ == 1) {
        MatchEventStackInMemberFunction(eventStack, this, &StatisticsCollector::OnFinishTemplateInstantiation);
    }
    MatchEventStackInMemberFunction(eventStack, this, &StatisticsCollector::OnFileParse);
    MatchEventStackInMemberFunction(eventStack, this, &StatisticsCollector::OnFunction);

    return AnalysisControl::CONTINUE;
}

AnalysisControl StatisticsCollector::OnSimpleEvent(const EventStack& eventStack)
{
    if (passNumber_ == 0) {
        MatchEventStackInMemberFunction(eventStack, this, &StatisticsCollector::OnSymbolName);
    }

    return AnalysisControl::CONTINUE;
}

void StatisticsCollector::OnSymbolName(const SE::SymbolName& symbolName)
{
    symbolNames_.emplace_back(symbolName.Key(), symbolName.Name());
}

void StatisticsCollector::OnFinishTemplateInstantiation(const A::Activity& parent, const A::TemplateInstantiationGroup& templateInstantiationGroup)
{
    uint64_t primaryKey = templateInstantiationGroup.Back().PrimaryTemplateSymbolKey();
    uint64_t spentTime = templateInstantiationGroup.Back().ExclusiveCPUTime().count();

    std::pair<uint64_t, std::string> key(primaryKey, "");
    size_t pos = std::lower_bound(symbolNames_.begin(), symbolNames_.end(), key) - symbolNames_.begin();
    if (pos == symbolNames_.size() || symbolNames_[pos].first != primaryKey)
        return;   //not even registered

    instantiationTimes_[pos].totalTime_ += spentTime;
    instantiationTimes_[pos].count_++;
}

void StatisticsCollector::OnFileParse(const A::FrontEndFileGroup& files)
{
    if (passNumber_ == 0) {
        parsedFilePaths_.push_back(files.Back().Path());
    }
    if (passNumber_ == 1) {
        std::string path = files.Back().Path();
        uint64_t spentTime = files.Back().ExclusiveCPUTime().count();

        size_t pos = std::lower_bound(parsedFilePaths_.begin(), parsedFilePaths_.end(), path) - parsedFilePaths_.begin();
        if (pos == parsedFilePaths_.size() || parsedFilePaths_[pos] != path)
            return; //not even registered

        parseTimes_[pos].totalTime_ += spentTime;
        parseTimes_[pos].count_++;
    }
}

void StatisticsCollector::OnFunction(const A::Function& function)
{
    if (passNumber_ == 0) {
        functionNamesDecorated_.push_back(function.Name());
    }
    if (passNumber_ == 1) {
        std::string decoratedName = function.Name();
        uint64_t spentTime = function.ExclusiveCPUTime().count();

        size_t pos = std::lower_bound(functionNamesDecorated_.begin(), functionNamesDecorated_.end(), decoratedName) - functionNamesDecorated_.begin();
        if (pos == functionNamesDecorated_.size() || functionNamesDecorated_[pos] != decoratedName)
            return;   //not even registered

        generationTimes_[pos].totalTime_ += spentTime;
        generationTimes_[pos].count_++;
    }
}
