#pragma once

#include <unordered_set>
#include <cassert>
#include "VcperfBuildInsights.h"

namespace vcperf
{

// Linker invocations sometimes restart themselves. This can happen in the
// following cases:
//
// 1. The /LTCG switch was not used, and an object compiled with /GL was found.
//    The linker is restarted with /LTCG.
// 2. A 32-bit linker runs out of memory address space. The linker is restarted
//    in 64-bit.
// 
// These restarts may have a non-negligible impact on throughput so we would
// like to identify them automatically. The following C++ Build Insights
// analyzer implements this functionality.
class RestartedLinkerDetector : public BI::IAnalyzer
{
public:
    RestartedLinkerDetector():
        pass_{0},
        restartedLinkers_{}
    {}

    // Some analyses are done in multiple passes. The OnBeginAnalysisPass 
    // function comes from the IAnalyzer interface, and will be called every
    // time a pass begins.
    BI::AnalysisControl OnBeginAnalysisPass() override
    {
        // Let's keep track of the pass we are in.
        ++pass_;

        // We return CONTINUE to pass control over to the next
        // analyzer in the analysis chain.
        return BI::AnalysisControl::CONTINUE;
    }

    // The OnStartActivity function will be called every time an activity start
    // event is seen. An activity is something happening in MSVC that has a 
    // start and a stop time. The list of activities that generate start
    // events can be found in the Microsoft::Cpp::BuildInsights::Activities
    // namespace.
    BI::AnalysisControl OnStartActivity(const BI::EventStack& eventStack) override
    {
        // This analyzer is only active in the first pass. During this pass,
        // it will remember which linkers have been restarted by storing the
        // information in a cache.
        if (pass_ > 1) return BI::AnalysisControl::CONTINUE;

        // BACKGROUND:
        //
        // C++ Build Insights events are related to each other in the
        // form of a graph. Nodes are either 'activities' which have a start
        // and stop time, or 'simple events' which are punctual. Activity nodes 
        // may have children nodes. Simple events are always leaves. When an 
        // activity has a child node, it means that the child event occurred 
        // while the parent was still ongoing. For example, a Linker activity
        // may encapsulate the LinkerPass2 activity, which itself may 
        // encapsulate a LinkerLTCG activity. In this case, the graph would have
        // a branch that looks like: Linker --> LinkerPass2 --> LinkerLTCG.
        //
        // EVENT STACKS:
        //
        // The 'eventStack' parameter contains the path in the graph for the 
        // current event. Using the example above, if the current event is the
        // start of LinkerPass2, the event stack would contain:
        // [Linker, LinkerPass2], where LinkerPass2 is at the top of the stack.
        // You can think of the event stack as a call stack, because it tells
        // you what MSVC was currently executing, as well as how it got there.
        //
        // IDENTIFY RELEVANT BRANCHES:
        //
        // When writing a C++ Build Insights analyzer, the first thing you will
        // typically want to do is identify the branches in the graph that are 
        // relevant to your analysis. In our case, we want to identify linkers
        // that are being restarted (i.e. linkers that are encapsulated by 
        // another linker). This can be represented by a branch of this
        // form: X --> Linker --> X --> Linker, where the X's are activities we
        // don't care about.
        //
        // MATCHING A BRANCH
        //
        // Once the branch of interest has been identified, the next step is to
        // match the event stack against it. Here, this is done using
        // MatchEventStackInMemberFunction, which looks at a member function's 
        // parameters to determine the branch to match. The last parameter in 
        // the list is always the leaf, while others can be anywhere along the 
        // path from the root. In this case we are matching the stack in the 
        // FlagRestartedLinker member function. Its parameters describe a branch 
        // that ends in a Linker and that has a parent Linker somewhere along
        // the path. This is exactly what our analysis needs. 
        BI::MatchEventStackInMemberFunction(eventStack, this, 
            &RestartedLinkerDetector::FlagRestartedLinker);

        return BI::AnalysisControl::CONTINUE;
    }

    void FlagRestartedLinker(const A::Linker& parent, const A::Linker& child)
    {
        // Flag the parent linker as having been restarted
        // by inserting its instance ID into our cache.
        // In a given trace, each C++ Build Insights activity
        // or simple event has a unique instance ID. This ID is 
        // useful to correlate events between different analysis
        // passes for the same trace.
        restartedLinkers_.insert(parent.EventInstanceId());
    }

    // This function is intended to be called by other analyzers
    // in the chain if they need to know whether a linker was
    // restarted. Restarted linkers have already been flagged in pass 1, 
    // so we look into the cache to gather the information.
    bool WasLinkerRestarted(const A::Invocation& linker) const 
    {
        return restartedLinkers_.find(linker.EventInstanceId()) != 
            restartedLinkers_.end();
    }

private:
    unsigned pass_;
    std::unordered_set<unsigned long long> restartedLinkers_;
};

} // namespace vcperf