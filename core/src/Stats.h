// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__Stats__
#define __ODC__Stats__

// ODC
#include "Topology.h"
// STD
#include <vector>

namespace odc::core
{
    template <class State_t>
    struct SStateInfo
    {
        SStateInfo()
        {
        }

        SStateInfo(State_t _state)
            : m_state(_state)
        {
        }

        State_t m_state;
        std::set<uint64_t> m_ids;
    };

    using STaskState = SStateInfo<DeviceState>;
    using SCollectionState = SStateInfo<AggregatedTopologyState>;

    struct SStateStats
    {
        SStateStats(const FairMQTopologyState& _topoState);

        std::string toString() const;

        std::map<DeviceState, STaskState> m_tasks;
        std::map<AggregatedTopologyState, SCollectionState> m_collections;

        size_t m_taskCount{ 0 };
        size_t m_collectionCount{ 0 };
    };
}; // namespace odc::core

#endif /* defined(__ODC__Stats__) */
