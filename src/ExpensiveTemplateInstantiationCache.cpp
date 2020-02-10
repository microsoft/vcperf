#include "ExpensiveTemplateInstantiationCache.h"

std::tuple<bool, const char*, const char*> ExpensiveTemplateInstantiationCache::
    GetTemplateInstantiationInfo(const TemplateInstantiation& ti) const 
{
    auto itPrimary = keysToConsider_.find(ti.PrimaryTemplateSymbolKey());

    if (itPrimary == keysToConsider_.end()) {
        return { false, nullptr, nullptr };
    }

    auto itSpecialization = keysToConsider_.find(ti.SpecializationSymbolKey());

    if (itSpecialization == keysToConsider_.end()) 
    {
        return { true, itPrimary->second, "<unknown specialization>" };
    }

    return { true, itPrimary->second, itSpecialization->second };
}

AnalysisControl ExpensiveTemplateInstantiationCache::
    OnStopActivity(const EventStack& eventStack)
{
    if (!isEnabled_ || IsRelogging()) {
        return AnalysisControl::CONTINUE;
    }

    MatchEventStackInMemberFunction(eventStack, this, 
        &ExpensiveTemplateInstantiationCache::OnTemplateInstantiation);

    return AnalysisControl::CONTINUE;
}

AnalysisControl ExpensiveTemplateInstantiationCache::
    OnSimpleEvent(const EventStack& eventStack)
{
    if (!isEnabled_ || IsRelogging()) {
        return AnalysisControl::CONTINUE;
    }

    MatchEventStackInMemberFunction(eventStack, this, 
        &ExpensiveTemplateInstantiationCache::OnSymbolName);

    return AnalysisControl::CONTINUE;
}

AnalysisControl ExpensiveTemplateInstantiationCache::OnEndAnalysisPass()
{
    if (!isEnabled_ || IsRelogging()) {
        return AnalysisControl::CONTINUE;
    }

    switch (analysisPass_)
    {
    case 1:
        DetermineTopPrimaryTemplates();
        break;

    case 2:
    default:
        localSpecializationKeysToConsider_.clear();
        break;
    }

    return AnalysisControl::CONTINUE;
}

void ExpensiveTemplateInstantiationCache::
    OnTemplateInstantiation(const TemplateInstantiation& instantiation)
{
    switch (analysisPass_)
    {
    case 1:
        Phase1RegisterPrimaryTemplateLocalTime(instantiation);
        break;

    case 2:
        Phase2RegisterSpecializationKey(instantiation);
        break;

    default:
        break;
    }
}

void ExpensiveTemplateInstantiationCache::OnSymbolName(const SymbolName& symbol)
{
    switch (analysisPass_)
    {
    case 1:
        Phase1MergePrimaryTemplateDuration(symbol);
        break;

    case 2:
        Phase2MergeSpecializationKey(symbol);
        break;

    default:
        break;
    }
}

void ExpensiveTemplateInstantiationCache::
    Phase1RegisterPrimaryTemplateLocalTime(const TemplateInstantiation& instantiation)
{
    unsigned int microseconds = (unsigned int)std::chrono::
        duration_cast<std::chrono::microseconds>(instantiation.Duration()).count();

    localPrimaryTemplateTimes_[instantiation.PrimaryTemplateSymbolKey()] += microseconds;
}

void ExpensiveTemplateInstantiationCache::
    Phase1MergePrimaryTemplateDuration(const SymbolName& symbol)
{
    auto itPrimary = localPrimaryTemplateTimes_.find(symbol.Key());

    if (itPrimary != localPrimaryTemplateTimes_.end())
    {
        auto it = cachedSymbolNames_.insert(symbol.Name()).first;
        auto& stats = primaryTemplateStats_[it->c_str()];
        stats.PrimaryKeys.push_back(symbol.Key());
        stats.TotalMicroseconds += itPrimary->second;

        localPrimaryTemplateTimes_.erase(itPrimary);
    }
}

void ExpensiveTemplateInstantiationCache::
    Phase2RegisterSpecializationKey(const TemplateInstantiation& instantiation)
{
    if (keysToConsider_.find(instantiation.PrimaryTemplateSymbolKey()) != keysToConsider_.end()) {
        localSpecializationKeysToConsider_.insert(instantiation.SpecializationSymbolKey());
    }
}

void ExpensiveTemplateInstantiationCache::
    Phase2MergeSpecializationKey(const SymbolName& symbol)
{
    auto it = localSpecializationKeysToConsider_.find(symbol.Key());

    if (it != localSpecializationKeysToConsider_.end()) 
    {
        auto itCached = cachedSymbolNames_.insert(symbol.Name()).first;
        keysToConsider_[symbol.Key()] = itCached->c_str();

        localSpecializationKeysToConsider_.erase(it);
    }
}

void ExpensiveTemplateInstantiationCache::DetermineTopPrimaryTemplates()
{
    unsigned int cutoff = 500000;

    unsigned long long durationBasedCutoff = 
        (unsigned long long)(0.05 * traceDuration_.count());

    if (durationBasedCutoff < cutoff) {
        cutoff = (unsigned int)durationBasedCutoff;
    }

    for (auto& p : primaryTemplateStats_)
    {
        if (p.second.TotalMicroseconds >= cutoff)
        {
            for (auto primKey : p.second.PrimaryKeys) {
                keysToConsider_[primKey] = p.first;
            }

            continue;
        }

        cachedSymbolNames_.erase(p.first);
    }

    primaryTemplateStats_.clear();
    localPrimaryTemplateTimes_.clear();
}