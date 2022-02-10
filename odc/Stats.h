/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

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

        std::string tasksString() const;
        std::string collectionsString() const;

        std::map<DeviceState, STaskState> m_tasks;
        std::map<AggregatedTopologyState, SCollectionState> m_collections;

        size_t m_taskCount{ 0 };
        size_t m_collectionCount{ 0 };
    };
} // namespace odc::core

#endif /* defined(__ODC__Stats__) */
