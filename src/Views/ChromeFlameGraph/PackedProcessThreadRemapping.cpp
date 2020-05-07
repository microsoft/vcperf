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
    assert(remappings_.empty());

    RemapRootsProcessId(hierarchy);
    RemapEntriesThreadId(hierarchy);
}

void PackedProcessThreadRemapping::CalculateChildrenLocalThreadData(const ExecutionHierarchy::Entry* entry)
{
    assert(entry->Children.size() > 0);

    CalculateChildrenLocalThreadId(entry);
    CalculateChildrenExtraThreadIdToFitHierarchy(entry);
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

                overlappingLocalThreadIds.push_back(it->second.LocalThreadId);
            }
        }

        // calculate first local ThreadId where we don't overlap any sibling
        unsigned long localThreadId = 0;
        while (std::find(overlappingLocalThreadIds.begin(), overlappingLocalThreadIds.end(), localThreadId) != overlappingLocalThreadIds.end())
        {
            ++localThreadId;
        }

        // store local ThreadId
        assert(localOffsetsData_.find(child->Id) == localOffsetsData_.end());
        LocalOffsetData& data = localOffsetsData_[child->Id];
        data.LocalThreadId = localThreadId;
    }
}

void PackedProcessThreadRemapping::CalculateChildrenExtraThreadIdToFitHierarchy(const ExecutionHierarchy::Entry* entry)
{
    assert(entry->Children.size() > 0);

    // group children with their partial local data
    typedef std::pair<const ExecutionHierarchy::Entry*, const LocalOffsetData*> EntryWithOffsetData;
    std::vector<EntryWithOffsetData> sortedChildrenWithData;
    for (const ExecutionHierarchy::Entry* child : entry->Children)
    {
        auto it = localOffsetsData_.find(child->Id);
        assert(it != localOffsetsData_.end());

        sortedChildrenWithData.emplace_back(child, &it->second);
    }

    // sort them by local ThreadId
    std::sort(sortedChildrenWithData.begin(), sortedChildrenWithData.end(),
              [](const EntryWithOffsetData& lhs, const EntryWithOffsetData& rhs)
    {
        return lhs.second->LocalThreadId < rhs.second->LocalThreadId;
    });

    // TODO: calculate extra ThreadId to fit hierarchy!
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
}
