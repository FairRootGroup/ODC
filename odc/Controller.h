/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_CONTROLLER
#define ODC_CORE_CONTROLLER

#include <odc/DDSSubmit.h>
#include <odc/Params.h>
#include <odc/Process.h>
#include <odc/Topology.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace odc::core
{

class Controller
{
  public:
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

    Controller() {}
    // Disable copy constructors and assignment operators
    Controller(const Controller&) = delete;
    Controller(Controller&&) = delete;
    Controller& operator=(const Controller&) = delete;
    Controller& operator=(Controller&&) = delete;

    /// \brief Set timeout of requests
    /// \param [in] timeout Timeout in seconds
    void setTimeout(const std::chrono::seconds& timeout) { mTimeout = timeout; }

    /// \brief Register resource plugins
    /// \param [in] pluginMap Map of plugin name to path
    void registerResourcePlugins(const PluginManager::PluginMap& pluginMap);

    /// \brief Register request triggers
    /// \param [in] triggerMap Map of plugin name to path
    void registerRequestTriggers(const PluginManager::PluginMap& triggerMap);

    /// \brief Restore sessions for the specified ID
    ///  The function has to be called before the service start accepting request.
    /// \param [in] id ID of the restore file
    /// \param [in] dir directory to store restore files in
    void restore(const std::string& id, const std::string& dir);

    /// \brief Set directory where history file is stored
    /// \param [in] dir directory path
    void setHistoryDir(const std::string& dir) { mHistoryDir = dir; }

    // DDS topology and session requests

    /// \brief Initialize DDS session
    RequestResult execInitialize(const CommonParams& common, const InitializeParams& params);
    /// \brief Submit DDS agents. Can be called multiple times in order to submit more agents.
    RequestResult execSubmit(const CommonParams& common, const SubmitParams& params);
    /// \brief Activate topology
    RequestResult execActivate(const CommonParams& common, const ActivateParams& params);
    /// \brief Run request combines Initialize, Submit and Activate
    RequestResult execRun(const CommonParams& common, const RunParams& params);
    /// \brief Update topology. Can be called multiple times in order to update topology.
    RequestResult execUpdate(const CommonParams& common, const UpdateParams& params);
    /// \brief Shutdown DDS session
    RequestResult execShutdown(const CommonParams& common);

    /// \brief Set properties
    RequestResult execSetProperties(const CommonParams& common, const SetPropertiesParams& params);
    /// \brief Get state
    RequestResult execGetState(const CommonParams& common, const DeviceParams& params);

    // change state requests

    /// \brief Configure devices: InitDevice->CompleteInit->Bind->Connect->InitTask
    RequestResult execConfigure(const CommonParams& common, const DeviceParams& params);
    /// \brief Start devices: Run
    RequestResult execStart(const CommonParams& common, const DeviceParams& params);
    /// \brief Stop devices: Stop
    RequestResult execStop(const CommonParams& common, const DeviceParams& params);
    /// \brief Reset devices: ResetTask->ResetDevice
    RequestResult execReset(const CommonParams& common, const DeviceParams& params);
    /// \brief Terminate devices: End
    RequestResult execTerminate(const CommonParams& common, const DeviceParams& params);

    /// \brief Status request
    StatusRequestResult execStatus(const StatusParams& params);

  private:
    std::map<std::string, std::unique_ptr<Session>> mSessions; ///< Map of partition ID to session info
    std::mutex mSessionsMtx;                                   ///< Mutex of sessions map
    std::chrono::seconds mTimeout{ 30 };                       ///< Request timeout in sec
    DDSSubmit mSubmit;                                         ///< ODC to DDS submit resource converter
    PluginManager mTriggers;                                   ///< Request triggers
    std::string mRestoreId;                                    ///< Restore ID
    std::string mRestoreDir;                                   ///< Restore file directory
    std::string mHistoryDir;                                   ///< History file directory

    void execRequestTrigger(const std::string& plugin, const CommonParams& common);
    void updateRestore();
    void updateHistory(const CommonParams& common, const std::string& sessionId);

    RequestResult createRequestResult(const CommonParams& common, const Error& error, const std::string& msg, size_t execTime, AggregatedState aggrState, std::unique_ptr<DetailedState> detailedState = nullptr);
    bool createDDSSession(const CommonParams& common, Error& error);
    bool attachToDDSSession(const CommonParams& common, Error& error, const std::string& sessionID);
    void submit(const CommonParams& common, Session& session, Error& error, const std::string& plugin, const std::string& res);
    bool submitDDSAgents(const CommonParams& common, Session& session, Error& error, const DDSSubmit::Params& params);
    void activate(const CommonParams& common, Session& session, Error& error);
    bool activateDDSTopology(const CommonParams& common, Error& error, const std::string& topologyFile, dds::tools_api::STopologyRequest::request_t::EUpdateType updateType);
    bool waitForNumActiveSlots(const CommonParams& common, Session& session, Error& error, size_t numSlots);
    bool requestCommanderInfo(const CommonParams& common, Error& error, dds::tools_api::SCommanderInfoRequest::response_t& commanderInfo);
    bool shutdownDDSSession(const CommonParams& common, Error& error);
    bool resetTopology(Session& session);
    bool createDDSTopology(const CommonParams& common, Error& error, const std::string& topologyFile);
    bool createTopology(const CommonParams& common, Error& error);
    bool setProperties(const CommonParams& common, Error& error, const SetPropertiesParams& params, AggregatedState& aggrState);
    bool changeState(const CommonParams& common, Session& session, Error& error, TopoTransition transition, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);
    bool changeStateConfigure(const CommonParams& common, Session& session, Error& error, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);
    bool changeStateReset(const CommonParams& common, Session& session, Error& error, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);
    bool waitForState(const CommonParams& common, Session& session, Error& error, DeviceState expState, const std::string& path);

    bool getState(const CommonParams& common, Error& error, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);

    void fillAndLogError(               const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void fillAndLogFatalError(          const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void fillAndLogFatalErrorLineByLine(const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void logFatalLineByLine(const CommonParams& common, const std::string& msg);

    AggregatedState aggregateStateForPath(const dds::topology_api::CTopology* ddsTopo, const TopoState& topoState, const std::string& path);

    Session& getOrCreateSession(const CommonParams& common);
    void removeSession(const CommonParams& common);

    FailedTasksCollections stateSummaryOnFailure(const CommonParams& common, Session& session, const TopoState& topoState, DeviceState expectedState);
    bool attemptStateRecovery(const CommonParams& common, Session& session, FailedTasksCollections& failed);
    void attemptSubmitRecovery(const CommonParams& common, Session& session, Error& error, const std::vector<DDSSubmit::Params>& ddsParams, const std::map<std::string, uint32_t>& agentCounts);
    void updateTopology(const CommonParams& common, Session& session);

    bool subscribeToDDSSession(const CommonParams& common, Error& error);

    std::string topoFilepath(const CommonParams& common, const std::string& topologyFile, const std::string& topologyContent, const std::string& topologyScript);

    std::chrono::seconds requestTimeout(const CommonParams& common) const
    {
        std::chrono::seconds configuredTimeoutS = (common.mTimeout == 0 ? mTimeout : std::chrono::seconds(common.mTimeout));
        std::chrono::milliseconds configuredTimeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(configuredTimeoutS);
        // subtract time elapsed since the beginning of the request
        std::chrono::milliseconds realTimeoutMs = configuredTimeoutMs - std::chrono::milliseconds(common.mTimer.duration());
        OLOG(debug, common) << "Configured request timeout: " << configuredTimeoutMs.count() << "ms "
            << (common.mTimeout == 0 ? "(controller default)" : "(request parameter)")
            << ", remaining time: " << realTimeoutMs.count() << "ms";
        return std::chrono::duration_cast<std::chrono::seconds>(realTimeoutMs);
    }

    uint32_t getNumSlots(const CommonParams& common, Session& session) const;
    dds::tools_api::SAgentInfoRequest::responseVector_t getAgentInfo(const CommonParams& common, Session& session) const;
    void extractRequirements(const CommonParams& common, Session& session);

    void printStateStats(const CommonParams& common, const TopoState& topoState);
};

} // namespace odc::core

#endif /* defined(ODC_CORE_CONTROLLER) */
