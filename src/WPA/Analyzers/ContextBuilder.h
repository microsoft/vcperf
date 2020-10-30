#pragma once

#include <vector>
#include <queue>
#include <string>
#include <set>
#include <unordered_map>
#include <cassert>

#include "VcperfBuildInsights.h"
#include "Utility.h"
#include "PayloadBuilder.h"

namespace vcperf
{

class ContextBuilder : public BI::IAnalyzer
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

    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnStopActivity(const BI::EventStack& eventStack) override;
    BI::AnalysisControl OnSimpleEvent(const BI::EventStack& eventStack) override;

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

    void OnLibOutput(const A::LinkerGroup& linkers, const SE::LibOutput& output);
    void OnExecutableImageOutput(const A::LinkerGroup& linkers, const SE::ExecutableImageOutput& output);
    void OnImpLibOutput(const A::LinkerGroup& linkers, const SE::ImpLibOutput& output);
    void OnCompilerInput(const A::Compiler& cl, const SE::FileInput& input);
    void OnCompilerOutput(const A::Compiler& cl, const SE::ObjOutput& output);

    void ProcessLinkerOutput(const A::LinkerGroup& linkers, const SE::FileOutput& output, bool overwrite);

    void OnRootActivity(const A::Activity& root);
    void OnNestedActivity(const A::Activity& parent, const A::Activity& child);
    void OnInvocation(const A::Invocation& invocation);
    void OnCompilerPass(const A::Compiler& cl, const A::CompilerPass& pass);
    void OnC2Thread(const A::C2DLL& c2, const A::Activity& threadOwner, const A::Thread& thread);

    void ProcessParallelismForkPoint(const A::Activity& parent, const A::Activity& child);
    void ProcessParallelismForkPoint(ContextLink& parentContextLink, const A::Activity& child);

    void OnStopRootActivity(const A::Activity& activity);
    void OnStopNestedActivity(const A::Activity& parent, const A::Activity& child);
    void OnStopCompilerPass(const A::CompilerPass& pass);
    void OnStopInvocation(const A::Invocation& invocation);
    void OnStopC2Thread(const A::C2DLL& c2, const A::Thread& thread);

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

} // namespace vcperf