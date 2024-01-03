/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_SESSION
#define ODC_CORE_SESSION

#include <odc/TopologyDefs.h>

#include <dds/Tools.h>
#include <dds/Topology.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace odc::core
{

struct Session
{
    TaskDetails& getTaskDetails(uint64_t taskID)
    {
        auto it = mTaskDetails.find(taskID);
        if (it == mTaskDetails.end()) {
            throw std::runtime_error(toString("Failed to get additional task info for ID (", taskID, ")"));
        }
        return it->second;
    }

    CollectionDetails& getCollectionDetails(uint64_t collectionID)
    {
        auto it = mCollectionDetails.find(collectionID);
        if (it == mCollectionDetails.end()) {
            throw std::runtime_error(toString("Failed to get additional collection info for ID (", collectionID, ")"));
        }
        return it->second;
    }

    void fillDetailedState(const TopoState& topoState, DetailedState& detailedState)
    {
        detailedState.clear();
        detailedState.reserve(topoState.size());

        for (const auto& state : topoState) {
            auto it = mTaskDetails.find(state.taskId);
            if (it != mTaskDetails.end()) {
                detailedState.emplace_back(DetailedTaskStatus(state, it->second.mPath, it->second.mHost));
            } else {
                detailedState.emplace_back(DetailedTaskStatus(state, "unknown", "unknown"));
            }
        }
    }

    void debug()
    {
        OLOG(info) << "tasks:";
        for (const auto& t : mTaskDetails) {
            OLOG(info) << t.second;
        }
        OLOG(info) << "collections:";
        for (const auto& c : mCollectionDetails) {
            OLOG(info) << c.second;
        }
    }

    std::unique_ptr<dds::topology_api::CTopology> mDDSTopo = nullptr; ///< DDS topology
    dds::tools_api::CSession mDDSSession; ///< DDS session
    std::string mPartitionID; ///< External partition ID of this DDS session
    std::string mTopoFilePath;
    std::map<std::string, CollectionNInfo> mNinfo; ///< Holds information on minimum number of collections, by collection name
    std::map<std::string, std::vector<ZoneGroup>> mZoneInfo; ///< Zones info zoneName:vector<ZoneGroup>
    std::unordered_map<std::string, AgentGroupInfo> mAgentGroupInfo; ///< Agent group info groupName:AgentGroupInfo
    std::vector<TaskInfo> mStandaloneTasks; ///< Standalone tasks (not belonging to any collection)
    std::map<std::string, CollectionInfo> mCollections; ///< Collection info collectionName:CollectionInfo
    std::unordered_map<uint64_t, CollectionInfo*> mRuntimeCollectionIndex; ///< Collection index by collection ID
    std::unordered_set<uint64_t> mExpendableTasks; ///< List of expandable task IDs
    size_t mTotalSlots = 0; ///< total number of DDS slots
    std::unordered_map<uint64_t, uint32_t> mAgentSlots;
    bool mRunAttempted = false;
    dds::tools_api::SOnTaskDoneRequest::ptr_t mDDSOnTaskDoneRequest;
    std::atomic<uint64_t> mLastRunNr = 0;
    std::unordered_map<uint64_t, TaskDetails> mTaskDetails; ///< Additional information about task
    std::unordered_map<uint64_t, CollectionDetails> mCollectionDetails; ///< Additional information about collection
};

} // namespace odc::core

#endif /* defined(ODC_CORE_SESSION) */
