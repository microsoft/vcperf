#pragma once

#include <vector>
#include <queue>
#include <string>
#include <set>
#include <unordered_map>
#include <assert.h>

#include "CppBuildInsights.hpp"
#include "Utility.h"
#include "PayloadBuilder.h"

using namespace Microsoft::Cpp::BuildInsights;
using namespace Activities;
using namespace SimpleEvents;

class ContextBuilder : public IAnalyzer
{
    struct Component
    {
        std::wstring Path;
        bool IsInput;
    };

public:
    struct ContextData
    {
        unsigned short          TimelineId;
        const char*             TimelineDescription;
        const char*             Tool;
        unsigned int            InvocationId;
        const wchar_t*          InvocationDescription;
        const wchar_t*          Component;
    };

private:
    struct ContextLink
    {
        ContextLink(ContextData* linkedContext, unsigned short timelineReusedCount):
            LinkedContext{linkedContext},
            TimelineReuseCount{timelineReusedCount}
        {}

        ContextData*            LinkedContext;
        unsigned short          TimelineReuseCount;
    };

    typedef std::unordered_map<unsigned long long, ContextData> ContextDataMap;
    typedef std::unordered_map<unsigned long long, ContextLink> ContextLinkMap;

public:
    ContextBuilder() :
        analysisCount_{0},
        analysisPass_{0},
        timelineCount_{0},
        contextData_{},
        activityContextLinks_{},
        availableTimelineIds_{},
        mainComponentCache_{},
        activeComponents_{},
        invocationDescriptions_{},
        timelineDescriptions_{},
        currentContextData_{nullptr},
        currentInstanceId_{0}
    {
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

    AnalysisControl OnStartActivity(const EventStack& eventStack) override;
    AnalysisControl OnStopActivity(const EventStack& eventStack) override;
    AnalysisControl OnSimpleEvent(const EventStack& eventStack) override;

    const ContextData* GetContextData() 
    {
        if (currentContextData_) {
            return currentContextData_;
        }

        if (currentInstanceId_ == 0) {
            return nullptr;
        }

        return GetContextLink(currentInstanceId_)->second.LinkedContext;
    }

private:

    bool MustCacheMainComponents() const {
        return analysisPass_ == 1;
    }

    bool MustBuildContext() const 
    {
        if (IsRelogging())
        {
            // At least one analysis pass is required to cache the main component paths.
            assert(analysisPass_ > 1);
            return true;
        }

        return false;
    }

    bool IsRelogging() const {
        return analysisCount_ == 1;
    }

    void OnLibOutput(const LinkerGroup& linkers, const LibOutput& output);
    void OnExecutableImageOutput(const LinkerGroup& linkers, const ExecutableImageOutput& output);
    void OnImpLibOutput(const LinkerGroup& linkers, const ImpLibOutput& output);
    void OnCompilerInput(const Compiler& cl, const FileInput& input);
    void OnCompilerOutput(const Compiler& cl, const ObjOutput& output);

    void ProcessLinkerOutput(const LinkerGroup& linkers, const FileOutput& output, bool overwrite);

    void OnRootActivity(const Activity& root);
    void OnNestedActivity(const Activity& parent, const Activity& child);
    void OnInvocation(const Invocation& invocation);
    void OnCompilerPass(const Compiler& cl, const CompilerPass& pass);
    void OnC2Thread(const C2DLL& c2, const Activity& threadOwner, const Thread& thread);

    void ProcessParallelismForkPoint(const Activity& parent, const Activity& child);
    void ProcessParallelismForkPoint(ContextLink& parentContextLink, const Activity& child);

    void OnStopRootActivity(const Activity& activity);
    void OnStopNestedActivity(const Activity& parent, const Activity& child);
    void OnStopCompilerPass(const CompilerPass& pass);
    void OnStopInvocation(const Invocation& invocation);
    void OnStopC2Thread(const C2DLL& c2, const Thread& thread);

    unsigned short GetNewTimelineId();

    ContextLinkMap::iterator GetContextLink(unsigned long long instanceId)
    {
        auto it = activityContextLinks_.find(instanceId);

        assert(it != activityContextLinks_.end());

        return it;
    }

    template <typename TChar>
    const TChar* CacheString(std::unordered_map<unsigned long long, std::basic_string<TChar>>& cache, 
        unsigned long long instanceId, const TChar* value)
    {
        return CacheString(cache, instanceId, std::basic_string<TChar>{value});
    }

    template <typename TChar>
    const TChar* CacheString(std::unordered_map<unsigned long long, std::basic_string<TChar>>& cache, 
        unsigned long long instanceId, std::basic_string<TChar>&& value)
    {
        auto result = cache.emplace(instanceId, std::move(value));

        assert(result.second);

        return result.first->second.c_str();
    }

    int analysisCount_;
    int analysisPass_;

    unsigned int timelineCount_;

    ContextDataMap contextData_;
    ContextLinkMap activityContextLinks_;

    std::priority_queue<
        unsigned short,
        std::vector<unsigned short>,
        std::greater<unsigned short>> availableTimelineIds_;

    std::unordered_map<unsigned long long, Component> mainComponentCache_;
    std::unordered_map<unsigned long long, std::wstring> activeComponents_;
    std::unordered_map<unsigned long long, std::wstring> invocationDescriptions_;
    std::unordered_map<unsigned short, std::string> timelineDescriptions_;

    ContextData* currentContextData_;
    unsigned long long currentInstanceId_;
};