#include "PackedProcessThreadRemapping.h"

#include <assert.h>

#include "Analyzers\ExecutionHierarchy.h"

using namespace vcperf;

PackedProcessThreadRemapping::PackedProcessThreadRemapping() :
    remappings_{}
{
}

void PackedProcessThreadRemapping::Calculate(const ExecutionHierarchy* hierarchy)
{
    assert(remappings_.empty());

    RemapRootsProcessId(hierarchy);
    RemapEntriesThreadIds(hierarchy);
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

        // entries are sorted in time, so we only have to check previous siblings for overlaps
        overlappingProcessIds.clear();
        for (int precedingSiblingIndex = rootIndex - 1; precedingSiblingIndex >= 0; --precedingSiblingIndex)
        {
            const ExecutionHierarchy::Entry* precedingSibling = roots[precedingSiblingIndex];

            if (root->OverlapsWith(precedingSibling))
            {
                // preceding sibling has been processed already, so we must find a remap for it
                auto precedingSiblingRemap = remappings_.find(precedingSibling->Id);
                assert(precedingSiblingRemap != remappings_.end());

                overlappingProcessIds.push_back(precedingSiblingRemap->second.ProcessId);
            }
        }

        // calculate first ProcessId where we don't overlap with any sibling
        unsigned long remappedProcessId = 0;
        while (std::find(overlappingProcessIds.begin(), overlappingProcessIds.end(), remappedProcessId) != overlappingProcessIds.end())
        {
            ++remappedProcessId;
        }

        // roots always get assigned to the lowest ThreadId
        remappings_.try_emplace(root->Id, Remap{ remappedProcessId, 0 });
    }
}

void PackedProcessThreadRemapping::RemapEntriesThreadId(const ExecutionHierarchy* hierarchy)
{
}
