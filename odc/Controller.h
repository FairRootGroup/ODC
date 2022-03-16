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
#include <odc/Process.h>
#include <odc/Topology.h>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>

namespace odc::core
{

/// \brief Return status code of request
enum StatusCode
{
    unknown = 0,
    ok,
    error
};

/// \brief General error
struct Error
{
    Error() {}
    Error(std::error_code code, const std::string& details)
        : m_code(code)
        , m_details(details)
    {}

    std::error_code m_code; ///< Error code
    std::string m_details;  ///< Details of the error

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const Error& error) { return os << error.m_code << " (" << error.m_details << ")"; }
};

/// \brief Holds device status of detailed output
struct TaskStatus
{
    TaskStatus() {}
    TaskStatus(const DeviceStatus& status, const std::string& path)
        : m_status(status)
        , m_path(path)
    {}

    DeviceStatus m_status;
    std::string m_path;
};

/// \brief Aggregated topology state
using TopologyState = std::vector<TaskStatus>;

enum class DDSSessionStatus
{
    unknown = 0,
    running = 1,
    stopped = 2
};

struct PartitionStatus
{
    PartitionStatus() {}
    PartitionStatus(const std::string& partitionID, const std::string& sessionID, DDSSessionStatus sessionStatus, AggregatedState aggregatedState)
        : m_partitionID(partitionID)
        , m_sessionID(sessionID)
        , m_sessionStatus(sessionStatus)
        , m_aggregatedState(aggregatedState)
    {}

    std::string m_partitionID;                                                      ///< Partition ID
    std::string m_sessionID;                                                        ///< Session ID of DDS
    DDSSessionStatus m_sessionStatus = DDSSessionStatus::unknown;                       ///< DDS session status
    AggregatedState m_aggregatedState = AggregatedState::Undefined; ///< Aggregated state of the affected divices
};

struct BaseRequestResult
{
    BaseRequestResult() {}
    BaseRequestResult(StatusCode statusCode, const std::string& msg, size_t execTime, const Error& error)
        : m_statusCode(statusCode)
        , m_msg(msg)
        , m_execTime(execTime)
        , m_error(error)
    {}
    StatusCode m_statusCode = StatusCode::unknown; ///< Operation status code
    std::string m_msg;                               ///< General message about the status
    size_t m_execTime = 0;                           ///< Execution time in milliseconds
    Error m_error;                                  ///< In case of error containes information about the error
};

/// \brief Request result
struct RequestResult : public BaseRequestResult
{
    RequestResult() {}
    RequestResult(StatusCode statusCode,
                 const std::string& msg,
                 size_t execTime,
                 const Error& error,
                 const std::string& partitionID,
                 uint64_t runNr,
                 const std::string& sessionID,
                 AggregatedState aggregatedState,
                 std::unique_ptr<TopologyState> fullState = nullptr)
        : BaseRequestResult(statusCode, msg, execTime, error)
        , m_partitionID(partitionID)
        , m_runNr(runNr)
        , m_sessionID(sessionID)
        , m_aggregatedState(aggregatedState)
        , mFullState(std::move(fullState))
    {}

    std::string m_partitionID;                                                      ///< Partition ID
    uint64_t m_runNr = 0;                                                           ///< Run number
    std::string m_sessionID;                                                        ///< Session ID of DDS
    AggregatedState m_aggregatedState = AggregatedState::Undefined; ///< Aggregated state of the affected divices

    // Optional parameters
    std::unique_ptr<TopologyState> mFullState = nullptr;
};

/// \brief Status Request Result
struct StatusRequestResult : public BaseRequestResult
{
    StatusRequestResult() {}
    StatusRequestResult(StatusCode statusCode, const std::string& msg, size_t execTime, const Error& error)
        : BaseRequestResult(statusCode, msg, execTime, error)
    {}

    std::vector<PartitionStatus> m_partitions; ///< Statuses of partitions
};

/// \brief Structure holds common request parameters
struct CommonParams
{
    CommonParams() {}
    CommonParams(const std::string& partitionID, uint64_t runNr, int timeout)
        : m_partitionID(partitionID)
        , m_runNr(runNr)
        , m_timeout(timeout)
    {}

    std::string m_partitionID; ///< Partition ID.
    uint64_t m_runNr = 0;      ///< Run number.
    size_t m_timeout = 0;      ///< Request timeout in seconds. 0 means "not set"

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const CommonParams& p)
    {
        return os << "CommonParams: partitionID=" << quoted(p.m_partitionID) << ", runNr=" << p.m_runNr << ", timeout=" << p.m_timeout;
    }
};

/// \brief Structure holds configuration parameters of the Initiaalize request
struct SInitializeParams
{
    SInitializeParams() {}
    SInitializeParams(const std::string& sessionID) : m_sessionID(sessionID) {}

    std::string m_sessionID; ///< DDS session ID

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SInitializeParams& p) { return os << "InitilizeParams: sid=" << quoted(p.m_sessionID); }
};

/// \brief Structure holds configuration parameters of the submit request
struct SSubmitParams
{
    SSubmitParams() {}
    SSubmitParams(const std::string& plugin, const std::string& resources)
        : m_plugin(plugin)
        , m_resources(resources)
    {}
    std::string m_plugin;    ///< ODC resource plugin name. Plugin has to be registered in ODC server.
    std::string m_resources; ///< Parcable description of the requested resources.

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SSubmitParams& p)
    {
        return os << "SubmitParams: plugin=" << quoted(p.m_plugin) << "; resources=" << quoted(p.m_resources);
    }
};

/// \brief Structure holds configuration parameters of the activate topology request
struct SActivateParams
{
    SActivateParams() {}
    SActivateParams(const std::string& topoFile, const std::string& topoContent, const std::string& topoScript)
        : m_topologyFile(topoFile)
        , m_topologyContent(topoContent)
        , m_topologyScript(topoScript)
    {}
    std::string m_topologyFile;    ///< Path to the topoloy file
    std::string m_topologyContent; ///< Content of the XML topology
    std::string m_topologyScript;  ///< Script that generates topology content

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SActivateParams& p)
    {
        return os << "ActivateParams: topologyFile=" << quoted(p.m_topologyFile) << "; topologyContent=" << quoted(p.m_topologyContent)
                << "; topologyScript=" << quoted(p.m_topologyScript);
    }
};

/// \brief Structure holds configuration parameters of the updatetopology request
struct SUpdateParams
{
    SUpdateParams() {}
    SUpdateParams(const std::string& topoFile, const std::string& topoContent, const std::string& topoScript)
        : m_topologyFile(topoFile)
        , m_topologyContent(topoContent)
        , m_topologyScript(topoScript)
    {}
    std::string m_topologyFile;    ///< Path to the topoloy file
    std::string m_topologyContent; ///< Content of the XML topology
    std::string m_topologyScript;  ///< Script that generates topology content

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SUpdateParams& p)
    {
        return os << "UpdateParams: topologyFile=" << quoted(p.m_topologyFile) << "; topologyContent=" << quoted(p.m_topologyContent)
                  << "; topologyScript=" << quoted(p.m_topologyScript);
    }
};

/// \brief Structure holds configuaration parameters of the SetProperties request
struct SetPropertiesParams
{
    using Property_t = std::pair<std::string, std::string>;
    using Properties_t = std::vector<Property_t>;

    SetPropertiesParams() {}
    SetPropertiesParams(const Properties_t& properties, const std::string& path)
        : m_path(path)
        , m_properties(properties)
    {}
    std::string m_path;        ///< Path in the topology
    Properties_t m_properties; ///< List of device configuration properties

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SetPropertiesParams& p)
    {
        os << "SetPropertiesParams: path=" << quoted(p.m_path) << "; properties={";
        for (const auto& v : p.m_properties) {
            os << " (" << v.first << ":" << v.second << ") ";
        }
        return os << "}";
    }
};

/// \brief Structure holds device state params used in FairMQ device state chenge requests.
struct SDeviceParams
{
    SDeviceParams() {}
    SDeviceParams(const std::string& path, bool detailed)
        : m_path(path)
        , m_detailed(detailed)
    {}
    std::string m_path;       ///< Path to the topoloy file
    bool m_detailed = false; ///< If True than return also detailed information

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SDeviceParams& p)
    {
        return os << "DeviceParams: path=" << quoted(p.m_path) << "; detailed=" << p.m_detailed;
    }
};

/// \brief Parameters of status request
struct SStatusParams
{
    SStatusParams() {}
    SStatusParams(bool running) : m_running(running) {}

    bool m_running = false; ///< Select only running DDS sessions

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SStatusParams& p) { return os << "StatusParams: running=" << p.m_running; }
};

struct TopoTaskInfo
{
    uint64_t mAgentID = 0; ///< Agent ID
    uint64_t mSlotID = 0;  ///< Slot ID
    uint64_t mTaskID = 0;  ///< Task ID, 0 if not assigned
    std::string mPath;     ///< Path in the topology
    std::string mHost;     ///< Hostname
    std::string mWrkDir;   ///< Wrk directory

    friend std::ostream& operator<<(std::ostream& os, const TopoTaskInfo& i)
    {
        return os << "agentID=" << i.mAgentID << ", slotID=" << i.mSlotID << ", taskID=" << i.mTaskID
                  << ", path=" << quoted(i.mPath) << ", host=" << i.mHost << ", wrkDir=" << quoted(i.mWrkDir);
    }
};

struct TopoCollectionInfo
{
    uint64_t mAgentID = 0; ///< Agent ID
    uint64_t mSlotID = 0;  ///< Slot ID
    uint64_t mCollectionID = 0;  ///< Task/Collection ID, 0 if not assigned
    std::string mPath;     ///< Path in the topology
    std::string mHost;     ///< Hostname
    std::string mWrkDir;   ///< Wrk directory

    friend std::ostream& operator<<(std::ostream& os, const TopoCollectionInfo& i)
    {
        return os << "agentID=" << i.mAgentID << ", slotID=" << i.mSlotID << ", collectionID=" << i.mCollectionID
                  << ", path=" << quoted(i.mPath) << ", host=" << i.mHost << ", wrkDir=" << quoted(i.mWrkDir);
    }
};

class Controller
{
  public:
    struct SessionInfo
    {
        std::unique_ptr<dds::topology_api::CTopology> m_topo = nullptr; ///< DDS topology
        std::shared_ptr<dds::tools_api::CSession> m_session = nullptr; ///< DDS session
        std::unique_ptr<Topology> m_fairmqTopology = nullptr; ///< FairMQ topology
        std::string m_partitionID; ///< External partition ID of this DDS session

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
    void setTimeout(const std::chrono::seconds& timeout) { m_timeout = timeout; }

    /// \brief Register resource plugins
    /// \param [in] pluginMap Map of plugin name to path
    void registerResourcePlugins(const CPluginManager::PluginMap_t& pluginMap);

    /// \brief Register request triggers
    /// \param [in] triggerMap Map of plugin name to path
    void registerRequestTriggers(const CPluginManager::PluginMap_t& triggerMap);

    /// \brief Restore sessions for the specified ID
    ///  The function has to be called before the service start accepting request.
    /// \param [in] id ID of the restore file
    void restore(const std::string& id);

    // DDS topology and session requests

    /// \brief Initialize DDS session
    RequestResult execInitialize(const CommonParams& common, const SInitializeParams& params);
    /// \brief Submit DDS agents. Can be called multiple times in order to submit more agents.
    RequestResult execSubmit(const CommonParams& common, const SSubmitParams& params);
    /// \brief Activate topology
    RequestResult execActivate(const CommonParams& common, const SActivateParams& params);
    /// \brief Run request combines Initialize, Submit and Activate
    RequestResult execRun(const CommonParams& common, const SInitializeParams& _initializeParams, const SSubmitParams& _submitParams, const SActivateParams& _activateParams);
    /// \brief Update topology. Can be called multiple times in order to update topology.
    RequestResult execUpdate(const CommonParams& common, const SUpdateParams& params);
    /// \brief Shutdown DDS session
    RequestResult execShutdown(const CommonParams& common);

    /// \brief Set properties
    RequestResult execSetProperties(const CommonParams& common, const SetPropertiesParams& params);
    /// \brief Get state
    RequestResult execGetState(const CommonParams& common, const SDeviceParams& params);

    // FairMQ device change state requests

    /// \brief Configure devices: InitDevice->CompleteInit->Bind->Connect->InitTask
    RequestResult execConfigure(const CommonParams& common, const SDeviceParams& params);
    /// \brief Start devices: Run
    RequestResult execStart(const CommonParams& common, const SDeviceParams& params);
    /// \brief Stop devices: Stop
    RequestResult execStop(const CommonParams& common, const SDeviceParams& params);
    /// \brief Reset devices: ResetTask->ResetDevice
    RequestResult execReset(const CommonParams& common, const SDeviceParams& params);
    /// \brief Terminate devices: End
    RequestResult execTerminate(const CommonParams& common, const SDeviceParams& params);

    // Generic requests

    /// \brief Status request
    StatusRequestResult execStatus(const SStatusParams& params);

  private:
    std::map<std::string, std::unique_ptr<SessionInfo>> m_sessions; ///< Map of partition ID to session info
    std::mutex mSessionsMtx; ///< Mutex of sessions map
    std::chrono::seconds m_timeout{ 30 }; ///< Request timeout in sec
    CDDSSubmit m_submit;                  ///< ODC to DDS submit resource converter
    CPluginManager m_triggers;            ///< Request triggers
    std::string m_restoreId;              ///< Restore ID

    void execRequestTrigger(const std::string& plugin, const CommonParams& common);
    void updateRestore();

    RequestResult createRequestResult(const CommonParams& common,
                                      const Error& error,
                                      const std::string& msg,
                                      size_t execTime,
                                      AggregatedState aggregatedState,
                                      std::unique_ptr<TopologyState> fullState = nullptr);
    bool createDDSSession(const CommonParams& common, Error& error);
    bool attachToDDSSession(const CommonParams& common, Error& error, const std::string& sessionID);
    bool submitDDSAgents(const CommonParams& common, Error& error, const CDDSSubmit::SParams& _params);
    bool activateDDSTopology(const CommonParams& common, Error& error, const std::string& topologyFile, dds::tools_api::STopologyRequest::request_t::EUpdateType updateType);
    bool waitForNumActiveAgents(const CommonParams& common, Error& error, size_t numAgents);
    bool requestCommanderInfo(const CommonParams& common, Error& error, dds::tools_api::SCommanderInfoRequest::response_t& commanderInfo);
    bool shutdownDDSSession(const CommonParams& common, Error& error);
    bool resetFairMQTopo(const CommonParams& common);
    bool createFairMQTopo(const CommonParams& common, Error& error, const std::string& topologyFile);
    bool createTopo(const CommonParams& common, Error& error, const std::string& topologyFile);
    bool setProperties(const CommonParams& common, Error& error, const SetPropertiesParams& params);
    bool changeState(const CommonParams& common,
                     Error& error,
                     TopologyTransition transition,
                     const std::string& path,
                     AggregatedState& aggregatedState,
                     TopologyState* topologyState = nullptr);
    bool getState(const CommonParams& common, Error& error, const std::string& path, AggregatedState& aggregatedState, TopologyState* topologyState = nullptr);
    bool changeStateConfigure(const CommonParams& common, Error& error, const std::string& path, AggregatedState& aggregatedState, TopologyState* topologyState = nullptr);
    bool changeStateReset(const CommonParams& common, Error& error, const std::string& path, AggregatedState& aggregatedState, TopologyState* topologyState = nullptr);

    void updateTopologyOnFailure();

    void fillError(const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void fillFatalError(const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);

    AggregatedState aggregateStateForPath(const dds::topology_api::CTopology* topo, const FairMQTopologyState& fairmq, const std::string& path);
    void fairMQToODCTopologyState(const dds::topology_api::CTopology* topo, const FairMQTopologyState& fairmq, TopologyState* odc);

    SessionInfo& getOrCreateSessionInfo(const CommonParams& common);

    Error checkSessionIsRunning(const CommonParams& common, ErrorCode errorCode);

    void printStateSummaryOnFailure(const CommonParams& common, const FairMQTopologyState& topologyState, DeviceState expectedState, SessionInfo& sessionInfo);

    bool subscribeToDDSSession(const CommonParams& common, Error& error);

    std::string topoFilepath(const CommonParams& common, const std::string& topologyFile, const std::string& topologyContent, const std::string& topologyScript);

    std::chrono::seconds requestTimeout(const CommonParams& common) const;
};
} // namespace odc::core

#endif /* defined(ODC_CORE_CONTROLLER) */
