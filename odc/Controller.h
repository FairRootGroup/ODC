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

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace odc::core
{

struct TopoGroupInfo
{
    uint64_t nOriginal;
    uint64_t nCurrent;
    uint64_t nMin;
};

struct FailedTasksCollections
{
    std::vector<TopoTaskInfo*> tasks;
    std::vector<TopoCollectionInfo*> collections;
};

class Controller
{
  public:
    struct SessionInfo
    {
        std::unique_ptr<dds::topology_api::CTopology> mDDSTopo = nullptr; ///< DDS topology
        std::shared_ptr<dds::tools_api::CSession> mDDSSession = nullptr; ///< DDS session
        std::unique_ptr<Topology> mTopology = nullptr; ///< Topology
        std::string mPartitionID; ///< External partition ID of this DDS session
        std::map<std::string, TopoGroupInfo> mNinfo; ///< Holds information on minimum number of groups, by group name
        std::string mTopoFilePath;

        void addToTaskCache(TopoTaskInfo&& taskInfo)
        {
            std::lock_guard<std::mutex> lock(mTaskCacheMutex);
            mTaskCache.emplace(taskInfo.mTaskID, taskInfo);
        }

        void addToCollectionCache(TopoCollectionInfo&& collectionInfo)
        {
            std::lock_guard<std::mutex> lock(mCollectionCacheMutex);
            mCollectionCache.emplace(collectionInfo.mCollectionID, collectionInfo);
        }

        TopoTaskInfo& getFromTaskCache(uint64_t taskID)
        {
            std::lock_guard<std::mutex> lock(mTaskCacheMutex);
            auto it = mTaskCache.find(taskID);
            if (it == mTaskCache.end()) {
                throw std::runtime_error(toString("Failed to get additional task info for ID (", taskID, ")"));
            }
            return it->second;
        }

        TopoCollectionInfo& getFromCollectionCache(uint64_t collectionID)
        {
            std::lock_guard<std::mutex> lock(mCollectionCacheMutex);
            auto it = mCollectionCache.find(collectionID);
            if (it == mCollectionCache.end()) {
                throw std::runtime_error(toString("Failed to get additional collection info for ID (", collectionID, ")"));
            }
            return it->second;
        }

        void debug()
        {
            {
                OLOG(info) << "tasks:";
                std::lock_guard<std::mutex> lock(mTaskCacheMutex);
                for (const auto& t : mTaskCache) {
                    OLOG(info) << t.second;
                }
            }
            {
                OLOG(info) << "collections:";
                std::lock_guard<std::mutex> lock(mCollectionCacheMutex);
                for (const auto& c : mCollectionCache) {
                    OLOG(info) << c.second;
                }
            }
        }

      private:
        std::mutex mTaskCacheMutex; ///< Mutex for the tasks container
        std::mutex mCollectionCacheMutex; ///< Mutex for the collections container
        std::unordered_map<uint64_t, TopoTaskInfo> mTaskCache; ///< Additional information about task
        std::unordered_map<uint64_t, TopoCollectionInfo> mCollectionCache; ///< Additional information about collection
    };

    Controller() {}
    // Disable copy constructors and assignment operators
    Controller(const Controller&) = delete;
    Controller(Controller&&) = delete;
    Controller& operator=(const Controller&) = delete;
    Controller& operator=(Controller&&) = delete;

    /// \brief Set timeout of requests
    /// \param [in] _timeout Timeout in seconds
    void setTimeout(const std::chrono::seconds& timeout) { mTimeout = timeout; }

    /// \brief Register resource plugins
    /// \param [in] pluginMap Map of plugin name to path
    void registerResourcePlugins(const PluginManager::PluginMap_t& pluginMap);

    /// \brief Register request triggers
    /// \param [in] triggerMap Map of plugin name to path
    void registerRequestTriggers(const PluginManager::PluginMap_t& triggerMap);

    /// \brief Restore sessions for the specified ID
    ///  The function has to be called before the service start accepting request.
    /// \param [in] id ID of the restore file
    void restore(const std::string& id);

    // DDS topology and session requests

    /// \brief Initialize DDS session
    RequestResult execInitialize(const CommonParams& common, const InitializeParams& params);
    /// \brief Submit DDS agents. Can be called multiple times in order to submit more agents.
    RequestResult execSubmit(const CommonParams& common, const SubmitParams& params);
    /// \brief Activate topology
    RequestResult execActivate(const CommonParams& common, const ActivateParams& params);
    /// \brief Run request combines Initialize, Submit and Activate
    RequestResult execRun(const CommonParams& common, const InitializeParams& initializeParams, const SubmitParams& submitParams, const ActivateParams& activateParams);
    /// \brief Update topology. Can be called multiple times in order to update topology.
    RequestResult execUpdate(const CommonParams& common, const UpdateParams& params);
    /// \brief Shutdown DDS session
    RequestResult execShutdown(const CommonParams& common);

    /// \brief Set properties
    RequestResult execSetProperties(const CommonParams& common, const SetPropertiesParams& params);
    /// \brief Get state
    RequestResult execGetState(const CommonParams& common, const DeviceParams& params);

    // FairMQ device change state requests

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

    // Generic requests

    /// \brief Status request
    StatusRequestResult execStatus(const StatusParams& params);

  private:
    std::map<std::string, std::unique_ptr<SessionInfo>> mSessions; ///< Map of partition ID to session info
    std::mutex mSessionsMtx; ///< Mutex of sessions map
    std::chrono::seconds mTimeout{ 30 }; ///< Request timeout in sec
    DDSSubmit mSubmit;                  ///< ODC to DDS submit resource converter
    PluginManager mTriggers;            ///< Request triggers
    std::string mRestoreId;              ///< Restore ID

    void execRequestTrigger(const std::string& plugin, const CommonParams& common);
    void updateRestore();

    RequestResult createRequestResult(const CommonParams& common, const Error& error, const std::string& msg, size_t execTime, AggregatedState aggrState, std::unique_ptr<DetailedState> detailedState = nullptr);
    bool createDDSSession(const CommonParams& common, Error& error);
    bool attachToDDSSession(const CommonParams& common, Error& error, const std::string& sessionID);
    bool submitDDSAgents(SessionInfo& sessionInfo, const CommonParams& common, Error& error, const DDSSubmit::Params& params);
    bool activateDDSTopology(const CommonParams& common, Error& error, const std::string& topologyFile, dds::tools_api::STopologyRequest::request_t::EUpdateType updateType);
    bool waitForNumActiveAgents(SessionInfo& sessionInfo, const CommonParams& common, Error& error, size_t numAgents);
    bool requestCommanderInfo(const CommonParams& common, Error& error, dds::tools_api::SCommanderInfoRequest::response_t& commanderInfo);
    bool shutdownDDSSession(const CommonParams& common, Error& error);
    bool resetTopology(const CommonParams& common);
    bool createTopology(const CommonParams& common, Error& error, const std::string& topologyFile);
    bool createDDSTopology(const CommonParams& common, Error& error, const std::string& topologyFile);
    bool setProperties(const CommonParams& common, Error& error, const SetPropertiesParams& params);
    bool changeState(const CommonParams& common, Error& error, TopologyTransition transition, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);
    bool changeStateConfigure(const CommonParams& common, Error& error, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);
    bool changeStateReset(const CommonParams& common, Error& error, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);

    bool getState(const CommonParams& common, Error& error, const std::string& path, AggregatedState& aggrState, DetailedState* detailedState = nullptr);

    void fillError(const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void fillFatalError(const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);

    AggregatedState aggregateStateForPath(const dds::topology_api::CTopology* ddsTopo, const TopologyState& topoState, const std::string& path);
    void getDetailedState(const dds::topology_api::CTopology* ddsTopo, const TopologyState& topoState, DetailedState* detailedState);

    SessionInfo& getOrCreateSessionInfo(const CommonParams& common);

    Error checkSessionIsRunning(const CommonParams& common, ErrorCode errorCode);

    FailedTasksCollections stateSummaryOnFailure(const CommonParams& common, const TopologyState& topoState, DeviceState expectedState, SessionInfo& sessionInfo);
    bool attemptRecovery(FailedTasksCollections& failed, SessionInfo& sessionInfo, const CommonParams& common);

    bool subscribeToDDSSession(const CommonParams& common, Error& error);

    std::string topoFilepath(const CommonParams& common, const std::string& topologyFile, const std::string& topologyContent, const std::string& topologyScript);

    std::chrono::seconds requestTimeout(const CommonParams& common) const;

    uint64_t getNumAgents(std::shared_ptr<dds::tools_api::CSession> ddsSession, const CommonParams& common) const;
};

} // namespace odc::core

#endif /* defined(ODC_CORE_CONTROLLER) */
