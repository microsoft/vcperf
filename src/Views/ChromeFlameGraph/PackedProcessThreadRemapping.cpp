#include "PackedProcessThreadRemapping.h"

#include <assert.h>
#include <algorithm>

#include "Analyzers\ExecutionHierarchy.h"

using namespace vcperf;

PackedProcessThreadRemapping::PackedProcessThreadRemapping() :
    remappings_{},
    localOffsetsData_{}
{
}

void PackedProcessThreadRemapping::Calculate(const ExecutionHierarchy* hierarchy)
{
    assert(hierarchy != nullptr);
    assert(remappings_.empty());

    RemapRootsProcessId(hierarchy);
    RemapEntriesThreadId(hierarchy);
}

void PackedProcessThreadRemapping::CalculateChildrenLocalThreadData(const ExecutionHierarchy::Entry* entry)
{
    assert(entry != nullptr);

    if (entry->Children.size() == 0)
    {
        // it's a leaf, so we're sure there's no data for it yet
        assert(localOffsetsData_.find(entry->Id) == localOffsetsData_.end());

        LocalOffsetData& data = localOffsetsData_[entry->Id];
        data.RawLocalThreadId = 0;  // parent will update this in CalculateChildrenLocalThreadId
        data.RequiredThreadIdToFitHierarchy = 0;
    }
    else
    {
        CalculateChildrenLocalThreadId(entry);
        CalculateChildrenExtraThreadIdToFitHierarchy(entry);
    }
}

void PackedProcessThreadRemapping::CalculateChildrenLocalThreadId(const ExecutionHierarchy::Entry* entry)
{
    assert(entry->Children.size() > 0);

    const std::vector<ExecutionHierarchy::Entry*>& children = entry->Children;
    std::vector<unsigned long> overlappingLocalThreadIds;
    for (int childIndex = 0; childIndex < children.size(); ++childIndex)
    {
        const ExecutionHierarchy::Entry* child = children[childIndex];

        // children are sorted by start time, so we only have to check previous siblings for overlaps
        overlappingLocalThreadIds.clear();
        for (int precedingSiblingIndex = childIndex - 1; precedingSiblingIndex >= 0; --precedingSiblingIndex)
        {
            const ExecutionHierarchy::Entry* precedingSibling = children[precedingSiblingIndex];

            if (child->OverlapsWith(precedingSibling))
            {
                // preceding sibling has been processed already, so we must find data for it
                auto it = localOffsetsData_.find(precedingSibling->Id);
                assert(it != localOffsetsData_.end());

                overlappingLocalThreadIds.push_back(it->second.RawLocalThreadId);
            }
        }

        // calculate first local ThreadId where we don't overlap any sibling
        unsigned long localThreadId = 0;
        while (std::find(overlappingLocalThreadIds.begin(), overlappingLocalThreadIds.end(), localThreadId) != overlappingLocalThreadIds.end())
        {
            ++localThreadId;
        }

        // update local ThreadId
        auto it = localOffsetsData_.find(child->Id);
        assert(it != localOffsetsData_.end());
        it->second.RawLocalThreadId = localThreadId;
    }
}

void PackedProcessThreadRemapping::CalculateChildrenExtraThreadIdToFitHierarchy(const ExecutionHierarchy::Entry* entry)
{
    assert(entry->Children.size() > 0);

    // group children with their partial LocalOffsetData
    typedef std::pair<const ExecutionHierarchy::Entry*, LocalOffsetData*> EntryWithOffsetData;
    std::vector<EntryWithOffsetData> sortedChildrenWithData;
    for (const ExecutionHierarchy::Entry* child : entry->Children)
    {
        auto it = localOffsetsData_.find(child->Id);
        assert(it != localOffsetsData_.end());

        sortedChildrenWithData.emplace_back(child, &it->second);
    }

    // sort them by their raw local ThreadId
    std::sort(sortedChildrenWithData.begin(), sortedChildrenWithData.end(),
              [](const EntryWithOffsetData& lhs, const EntryWithOffsetData& rhs)
    {
        return lhs.second->RawLocalThreadId < rhs.second->RawLocalThreadId;
    });

    // consider all children within the same local ThreadId as "sequential" and those
    // with a different ThreadId as "parallel"
    unsigned long currentLocalThreadId = sortedChildrenWithData[0].second->RawLocalThreadId;
    unsigned long currentExtraThreadToFitHierarchy = 0UL;
    unsigned long requiredExtraThreadIdToFitHierarchy = 0;
    for (EntryWithOffsetData& data : sortedChildrenWithData)
    {
        // when we iterate into a new local ThreadId, accumulate calculated data for previous "sequential" entries
        if (currentLocalThreadId != data.second->RawLocalThreadId)
        {
            requiredExtraThreadIdToFitHierarchy += currentExtraThreadToFitHierarchy;
        }

        // calculate the real local ThreadId: the raw one taking previous siblings' requirements into account
        data.second->CalculatedLocalThreadId = data.second->RawLocalThreadId + requiredExtraThreadIdToFitHierarchy;

        // "sequential" entries live in the same ThreadId, so we only need to account for the one that
        // needs more room for its subhierarchy
        if (currentLocalThreadId == data.second->RawLocalThreadId)
        {
            if (data.second->RequiredThreadIdToFitHierarchy > currentExtraThreadToFitHierarchy)
            {
                currentExtraThreadToFitHierarchy = data.second->RequiredThreadIdToFitHierarchy;
            }
        }
        // when we have reached a different ThreadId (moved into a "parallel" entry), prepare for next iteration
        else
        {
            currentLocalThreadId = data.second->RawLocalThreadId;
            currentExtraThreadToFitHierarchy = data.second->RequiredThreadIdToFitHierarchy;
        }
    }

    // last sequence of entries wasn't accumulated: we didn't iterate into a "next" ThreadId
    requiredExtraThreadIdToFitHierarchy += currentExtraThreadToFitHierarchy;

    // we're a parent, so we're sure we haven't been added to the map yet
    assert(localOffsetsData_.find(entry->Id) == localOffsetsData_.end());
    auto& data = localOffsetsData_[entry->Id];
    data.RawLocalThreadId = 0;  // our own parent will update this value taking our siblings into account
    data.RequiredThreadIdToFitHierarchy = currentLocalThreadId + requiredExtraThreadIdToFitHierarchy;
}

const PackedProcessThreadRemapping::Remap* PackedProcessThreadRemapping::GetRemapFor(unsigned long long id) const
{
    auto it = remappings_.find(id);
    return it != remappings_.end() ? &it->second : nullptr;
}

void PackedProcessThreadRemapping::RemapRootsProcessId(const ExecutionHierarchy* hierarchy)
{
    const ExecutionHierarchy::TRoots& roots = hierarchy->GetRoots();
    std::vector<unsigned long> overlappingProcessIds;
    for (int rootIndex = 0; rootIndex < roots.size(); ++rootIndex)
    {
        const ExecutionHierarchy::Entry* root = roots[rootIndex];

        // entries are sorted by start time, so we only have to check previous siblings for overlaps
        overlappingProcessIds.clear();
        for (int precedingSiblingIndex = rootIndex - 1; precedingSiblingIndex >= 0; --precedingSiblingIndex)
        {
            const ExecutionHierarchy::Entry* precedingSibling = roots[precedingSiblingIndex];

            if (root->OverlapsWith(precedingSibling))
            {
                // preceding sibling has been processed already, so we must find a remap for it
                auto it = remappings_.find(precedingSibling->Id);
                assert(it != remappings_.end());

                overlappingProcessIds.push_back(it->second.ProcessId);
            }
        }

        // calculate first ProcessId where we don't overlap with any sibling
        unsigned long remappedProcessId = 0;
        while (std::find(overlappingProcessIds.begin(), overlappingProcessIds.end(), remappedProcessId) != overlappingProcessIds.end())
        {
            ++remappedProcessId;
        }

        // roots always get assigned to the lowest ThreadId
        remappings_.try_emplace(root->Id, Remap{ remappedProcessId, 0UL });
    }
}

void PackedProcessThreadRemapping::RemapEntriesThreadId(const ExecutionHierarchy* hierarchy)
{
    for (const ExecutionHierarchy::Entry* root : hierarchy->GetRoots())
    {
        // this data must exist because we've already calculated ProcessId remappings
        // remember: parallel entries at root level is represented via different ProcessId
        auto it = remappings_.find(root->Id);
        assert(it != remappings_.end());

        RemapThreadIdFor(root, it->second.ProcessId, it->second.ThreadId);
    }
}

void PackedProcessThreadRemapping::RemapThreadIdFor(const ExecutionHierarchy::Entry* entry, unsigned long remappedProcessId,
                                                    unsigned long parentAbsoluteThreadId)
{
    for (const ExecutionHierarchy::Entry* child : entry->Children)
    {
        auto itLocalData = localOffsetsData_.find(child->Id);
        assert(itLocalData != localOffsetsData_.end());

        assert(remappings_.find(child->Id) == remappings_.end());
        Remap& remap = remappings_[child->Id];
        remap.ProcessId = remappedProcessId;
        remap.ThreadId = parentAbsoluteThreadId + itLocalData->second.CalculatedLocalThreadId;

        RemapThreadIdFor(child, remappedProcessId, remap.ThreadId);
    }
}
