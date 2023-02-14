/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_SESSION
#define ODC_CORE_SESSION

#include <odc/Topology.h>

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
    void addTaskDetails(TaskDetails&& taskDetails)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        mTaskDetails.emplace(taskDetails.mTaskID, taskDetails);
    }

    void addCollectionDetails(CollectionDetails&& collectionDetails)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        mCollectionDetails.emplace(collectionDetails.mCollectionID, collectionDetails);
    }

    void addAgentDetails(AgentDetails&& agentDetails)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        mAgentDetails.emplace(agentDetails.mID, agentDetails);
    }

    TaskDetails& getTaskDetails(uint64_t taskID)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        auto it = mTaskDetails.find(taskID);
        if (it == mTaskDetails.end()) {
            throw std::runtime_error(toString("Failed to get additional task info for ID (", taskID, ")"));
        }
        return it->second;
    }

    CollectionDetails& getCollectionDetails(uint64_t collectionID)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        auto it = mCollectionDetails.find(collectionID);
        if (it == mCollectionDetails.end()) {
            throw std::runtime_error(toString("Failed to get additional collection info for ID (", collectionID, ")"));
        }
        return it->second;
    }

    void fillDetailedState(const TopoState& topoState, DetailedState* detailedState)
    {
        if (detailedState == nullptr) {
            return;
        }

        detailedState->clear();
        detailedState->reserve(topoState.size());

        std::lock_guard<std::mutex> lock(mDetailsMtx);

        for (const auto& state : topoState) {
            auto it = mTaskDetails.find(state.taskId);
            if (it != mTaskDetails.end()) {
                detailedState->emplace_back(DetailedTaskStatus(state, it->second.mPath, it->second.mHost));
            } else {
                detailedState->emplace_back(DetailedTaskStatus(state, "unknown", "unknown"));
            }
        }
    }

    void fillAgentDetails(const std::unordered_map<uint64_t, AgentDetails>& agentDetails)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        mAgentDetails = agentDetails;
    }

    void addExpendableTask(uint64_t taskId)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        mExpendableTasks.emplace(taskId);
    }

    bool isTaskExpendable(uint64_t taskId)
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        return mExpendableTasks.count(taskId);
    }

    void clearExpendableTasks()
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        return mExpendableTasks.clear();
    }

    void debug()
    {
        OLOG(info) << "tasks:";
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        for (const auto& t : mTaskDetails) {
            OLOG(info) << t.second;
        }
        OLOG(info) << "collections:";
        for (const auto& c : mCollectionDetails) {
            OLOG(info) << c.second;
        }
        OLOG(info) << "agents:";
        for (const auto& a : mAgentDetails) {
            OLOG(info) << a.second;
        }
    }

    size_t numTasks()
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        return mTaskDetails.size();
    }

    size_t numCollections()
    {
        std::lock_guard<std::mutex> lock(mDetailsMtx);
        return mCollectionDetails.size();
    }

    std::unique_ptr<dds::topology_api::CTopology> mDDSTopo = nullptr; ///< DDS topology
    dds::tools_api::CSession mDDSSession; ///< DDS session
    std::unique_ptr<Topology> mTopology = nullptr; ///< Topology
    std::string mPartitionID; ///< External partition ID of this DDS session
    std::string mTopoFilePath;
    std::map<std::string, CollectionNInfo> mNinfo; ///< Holds information on minimum number of collections, by collection name
    std::map<std::string, std::vector<ZoneGroup>> mZoneInfos; ///< Zones info zoneName:vector<ZoneGroup>
    size_t mTotalSlots = 0; ///< total number of DDS slots
    std::unordered_map<uint64_t, uint32_t> mAgentSlots;
    bool mRunAttempted = false;
    dds::tools_api::SOnTaskDoneRequest::ptr_t mDDSOnTaskDoneRequest;
    std::atomic<uint64_t> mLastRunNr = 0;

    private:
    std::mutex mDetailsMtx; ///< Mutex for the tasks/collections container
    std::unordered_map<uint64_t, TaskDetails> mTaskDetails; ///< Additional information about task
    std::unordered_map<uint64_t, CollectionDetails> mCollectionDetails; ///< Additional information about collection
    std::unordered_map<uint64_t, AgentDetails> mAgentDetails; ///< Additional information about agent
    std::unordered_set<uint64_t> mExpendableTasks; ///< List of expandable task IDs
};

} // namespace odc::core

#endif /* defined(ODC_CORE_SESSION) */
