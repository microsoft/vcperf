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

    void OnLibOutput(LinkerGroup linkers, LibOutput output);
    void OnExecutableImageOutput(LinkerGroup linkers, ExecutableImageOutput output);
    void OnImpLibOutput(LinkerGroup linkers, ImpLibOutput output);
    void OnCompilerInput(Compiler cl, FileInput input);
    void OnCompilerOutput(Compiler cl, ObjOutput output);

    void ProcessLinkerOutput(const LinkerGroup& linkers, const FileOutput& output, bool overwrite);

    void OnRootActivity(Activity root);
    void OnNestedActivity(Activity parent, Activity child);
    void OnInvocation(Invocation invocation);
    void OnCompilerPass(Compiler cl, CompilerPass pass);
    void OnC2Thread(C2DLL c2, Activity threadOwner, Thread thread);

    void ProcessParallelismForkPoint(Activity parent, Activity child);
    void ProcessParallelismForkPoint(ContextLink& parentContextLink, const Activity& child);

    void OnStopRootActivity(Activity activity);
    void OnStopNestedActivity(Activity parent, Activity child);
    void OnStopCompilerPass(CompilerPass pass);
    void OnStopInvocation(Invocation invocation);
    void OnStopC2Thread(C2DLL c2, Thread thread);

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