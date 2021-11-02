// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "Stats.h"

using namespace std;
using namespace odc::core;

//
// SStateStats
//

SStateStats::SStateStats(const FairMQTopologyState& _topoState)
{
    for (const auto& v : _topoState)
    {
        if (m_tasks.find(v.state) == m_tasks.end())
        {
            m_tasks[v.state] = STaskState(v.state);
        }
        m_tasks[v.state].m_ids.insert(v.taskId);
    }

    auto collectionMap{ GroupByCollectionId(_topoState) };
    for (const auto& states : collectionMap)
    {
        auto collectionState{ AggregateState(states.second) };
        auto collectionId{ states.first };

        if (m_collections.find(collectionState) == m_collections.end())
        {
            m_collections[collectionState] = SCollectionState(collectionState);
        }
        m_collections[collectionState].m_ids.insert(collectionId);
    }

    m_taskCount = _topoState.size();
    m_collectionCount = collectionMap.size();
}

string SStateStats::toString() const
{
    stringstream ss;
    ss << "State stats for tasks:" << endl;
    for (const auto& v : m_tasks)
    {
        ss << right << setw(20) << fair::mq::GetStateName(v.first) << ": " << v.second.m_ids.size() << "/"
           << m_taskCount << endl;
    }
    ss << "State stats for collections:" << endl;
    for (const auto& v : m_collections)
    {
        ss << right << setw(20) << GetAggregatedTopologyStateName(v.first) << ": " << v.second.m_ids.size() << "/"
           << m_collectionCount << endl;
    }
    return ss.str();
}
