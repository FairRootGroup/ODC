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
#include <odc/Topology.h>
// STD
#include <map>
#include <set>
#include <string>
#include <sstream>

namespace odc::core {

template<class State>
struct StateInfo
{
    StateInfo() {}

    StateInfo(State state)
        : mState(state)
    {}

    State mState;
    std::set<uint64_t> mIds;
};

using TaskState = StateInfo<DeviceState>;
using CollectionState = StateInfo<AggregatedState>;

struct StateStats
{
    StateStats(const TopologyState& topoState)
    {
        for (const auto& v : topoState) {
            if (mTasks.find(v.state) == mTasks.end()) {
                mTasks[v.state] = TaskState(v.state);
            }
            mTasks[v.state].mIds.insert(v.taskId);
        }

        auto collectionMap{ GroupByCollectionId(topoState) };
        for (const auto& states : collectionMap) {
            auto collectionState{ AggregateState(states.second) };
            auto collectionId{ states.first };

            if (mCollections.find(collectionState) == mCollections.end()) {
                mCollections[collectionState] = CollectionState(collectionState);
            }
            mCollections[collectionState].mIds.insert(collectionId);
        }

        mTaskCount = topoState.size();
        mCollectionCount = collectionMap.size();
    }

    std::string tasksString() const
    {
        std::stringstream ss;
        ss << "Task states:";
        for (const auto& v : mTasks) {
            ss << " " << fair::mq::GetStateName(v.first) << " (" << v.second.mIds.size() << "/" << mTaskCount << ")";
        }
        return ss.str();
    }

    std::string collectionsString() const
    {
        std::stringstream ss;
        ss << "Collection states:";
        for (const auto& v : mCollections) {
            ss << " " << GetAggregatedStateName(v.first) << " (" << v.second.mIds.size() << "/" << mCollectionCount << ")";
        }
        return ss.str();
    }

    std::map<DeviceState, TaskState> mTasks;
    std::map<AggregatedState, CollectionState> mCollections;

    size_t mTaskCount = 0;
    size_t mCollectionCount = 0;
};
} // namespace odc::core

#endif /* defined(__ODC__Stats__) */
