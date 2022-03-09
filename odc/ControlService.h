/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__ControlService__
#define __ODC__ControlService__

// ODC
#include <odc/DDSSubmit.h>
#include <odc/Process.h>
#include <odc/Topology.h>
// BOOST
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
// STD
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>

namespace odc::core {

/// \brief Return status code of request
enum EStatusCode
{
    unknown = 0,
    ok,
    error
};

/// \brief General error
struct SError
{
    SError() {}
    SError(std::error_code _code, const std::string& _details)
        : m_code(_code)
        , m_details(_details)
    {}

    std::error_code m_code; ///< Error code
    std::string m_details;  ///< Details of the error

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& _os, const SError& error) { return _os << error.m_code << " (" << error.m_details << ")"; }
};

/// \brief Holds device status of detailed output
struct SDeviceStatus
{
    SDeviceStatus() {}
    SDeviceStatus(const DeviceStatus& _status, const std::string& _path)
        : m_status(_status)
        , m_path(_path)
    {}

    DeviceStatus m_status;
    std::string m_path;
};

/// \brief Aggregated topology state
using TopologyState = std::vector<SDeviceStatus>;

enum class ESessionStatus
{
    unknown = 0,
    running = 1,
    stopped = 2
};

struct SPartitionStatus
{
    SPartitionStatus() {}
    SPartitionStatus(const std::string& _partitionID, const std::string& _sessionID, ESessionStatus _sessionStatus, AggregatedTopologyState aggregatedState)
        : m_partitionID(_partitionID)
        , m_sessionID(_sessionID)
        , m_sessionStatus(_sessionStatus)
        , m_aggregatedState(aggregatedState)
    {}

    std::string m_partitionID;                                                      ///< Partition ID
    std::string m_sessionID;                                                        ///< Session ID of DDS
    ESessionStatus m_sessionStatus = ESessionStatus::unknown;                       ///< DDS session status
    AggregatedTopologyState m_aggregatedState = AggregatedTopologyState::Undefined; ///< Aggregated state of the affected divices
};

struct BaseRequestResult
{
    BaseRequestResult() {}
    BaseRequestResult(EStatusCode statusCode, const std::string& msg, size_t execTime, const SError& error)
        : m_statusCode(statusCode)
        , m_msg(msg)
        , m_execTime(execTime)
        , m_error(error)
    {}
    EStatusCode m_statusCode = EStatusCode::unknown; ///< Operation status code
    std::string m_msg;                               ///< General message about the status
    size_t m_execTime = 0;                           ///< Execution time in milliseconds
    SError m_error;                                  ///< In case of error containes information about the error
};

/// \brief Request result
struct RequestResult : public BaseRequestResult
{
    RequestResult() {}
    RequestResult(EStatusCode statusCode,
                 const std::string& msg,
                 size_t execTime,
                 const SError& error,
                 const std::string& _partitionID,
                 uint64_t _runNr,
                 const std::string& _sessionID,
                 AggregatedTopologyState aggregatedState,
                 std::unique_ptr<TopologyState> fullState = nullptr)
        : BaseRequestResult(statusCode, msg, execTime, error)
        , m_partitionID(_partitionID)
        , m_runNr(_runNr)
        , m_sessionID(_sessionID)
        , m_aggregatedState(aggregatedState)
        , mFullState(std::move(fullState))
    {}

    std::string m_partitionID;                                                      ///< Partition ID
    uint64_t m_runNr = 0;                                                           ///< Run number
    std::string m_sessionID;                                                        ///< Session ID of DDS
    AggregatedTopologyState m_aggregatedState = AggregatedTopologyState::Undefined; ///< Aggregated state of the affected divices

    // Optional parameters
    std::unique_ptr<TopologyState> mFullState = nullptr;
};

/// \brief Status Request Result
struct StatusRequestResult : public BaseRequestResult
{
    StatusRequestResult() {}
    StatusRequestResult(EStatusCode statusCode, const std::string& msg, size_t execTime, const SError& error)
        : BaseRequestResult(statusCode, msg, execTime, error)
    {}

    std::vector<SPartitionStatus> m_partitions; ///< Statuses of partitions
};

/// \brief Structure holds common request parameters
struct SCommonParams
{
    SCommonParams() {}
    SCommonParams(const std::string& _partitionID, uint64_t _runNr, int _timeout)
        : m_partitionID(_partitionID)
        , m_runNr(_runNr)
        , m_timeout(_timeout)
    {}

    std::string m_partitionID; ///< Partition ID.
    uint64_t m_runNr = 0;        ///< Run number.
    size_t m_timeout = 0;       ///< Request timeout in seconds. 0 means "not set"

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SCommonParams& p)
    {
        return os << "CommonParams: partitionID=" << quoted(p.m_partitionID) << ", runNr=" << p.m_runNr << ", timeout=" << p.m_timeout;
    }
};

/// \brief Structure holds configuration parameters of the Initiaalize request
struct SInitializeParams
{
    SInitializeParams() {}
    SInitializeParams(const std::string& _sessionID) : m_sessionID(_sessionID) {}

    std::string m_sessionID; ///< DDS session ID

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SInitializeParams& p) { return os << "InitilizeParams: sid=" << quoted(p.m_sessionID); }
};

/// \brief Structure holds configuration parameters of the submit request
struct SSubmitParams
{
    SSubmitParams() {}
    SSubmitParams(const std::string& _plugin, const std::string& _resources)
        : m_plugin(_plugin)
        , m_resources(_resources)
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
struct SSetPropertiesParams
{
    using Property_t = std::pair<std::string, std::string>;
    using Properties_t = std::vector<Property_t>;

    SSetPropertiesParams() {}
    SSetPropertiesParams(const Properties_t& _properties, const std::string& _path)
        : m_path(_path)
        , m_properties(_properties)
    {}
    std::string m_path;        ///< Path in the topology
    Properties_t m_properties; ///< List of device configuration properties

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const SSetPropertiesParams& p)
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
    SDeviceParams(const std::string& _path, bool _detailed)
        : m_path(_path)
        , m_detailed(_detailed)
    {}
    std::string m_path;       ///< Path to the topoloy file
    bool m_detailed{ false }; ///< If True than return also detailed information

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
    SStatusParams(bool _running) : m_running(_running) {}

    bool m_running{ false }; ///< Select only running DDS sessions

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

    friend std::ostream& operator<<(std::ostream& os, const TopoTaskInfo& i) {
        return os << "TopoTaskInfo: agentID=" << i.mAgentID << ", slotID=" << i.mSlotID << ", taskID=" << i.mTaskID
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

    friend std::ostream& operator<<(std::ostream& os, const TopoCollectionInfo& i) {
        return os << "TopoCollectionInfo: agentID=" << i.mAgentID << ", slotID=" << i.mSlotID << ", collectionID=" << i.mCollectionID
                  << ", path=" << quoted(i.mPath) << ", host=" << i.mHost << ", wrkDir=" << quoted(i.mWrkDir);
    }
};

class ControlService
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

    ControlService() {}
    // Disable copy constructors and assignment operators
    ControlService(const ControlService&) = delete;
    ControlService(ControlService&&) = delete;
    ControlService& operator=(const ControlService&) = delete;
    ControlService& operator=(ControlService&&) = delete;

    /// \brief Set timeout of requests
    /// \param [in] _timeout Timeout in seconds
    void setTimeout(const std::chrono::seconds& _timeout) { m_timeout = _timeout; }

    /// \brief Register resource plugins
    /// \param [in] _pluginMap Map of plugin name to path
    void registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap);

    /// \brief Register request triggers
    /// \param [in] _triggerMap Map of plugin name to path
    void registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap);

    /// \brief Restore sessions for the specified ID
    ///  The function has to be called before the service start accepting request.
    /// \param [in] _id ID of the restore file
    void restore(const std::string& _id);

    // DDS topology and session requests

    /// \brief Initialize DDS session
    RequestResult execInitialize(const SCommonParams& common, const SInitializeParams& _params);
    /// \brief Submit DDS agents. Can be called multiple times in order to submit more agents.
    RequestResult execSubmit(const SCommonParams& common, const SSubmitParams& _params);
    /// \brief Activate topology
    RequestResult execActivate(const SCommonParams& common, const SActivateParams& _params);
    /// \brief Run request combines Initialize, Submit and Activate
    RequestResult execRun(const SCommonParams& common, const SInitializeParams& _initializeParams, const SSubmitParams& _submitParams, const SActivateParams& _activateParams);
    /// \brief Update topology. Can be called multiple times in order to update topology.
    RequestResult execUpdate(const SCommonParams& common, const SUpdateParams& _params);
    /// \brief Shutdown DDS session
    RequestResult execShutdown(const SCommonParams& common);

    /// \brief Set properties
    RequestResult execSetProperties(const SCommonParams& common, const SSetPropertiesParams& _params);
    /// \brief Get state
    RequestResult execGetState(const SCommonParams& common, const SDeviceParams& _params);

    // FairMQ device change state requests

    /// \brief Configure devices: InitDevice->CompleteInit->Bind->Connect->InitTask
    RequestResult execConfigure(const SCommonParams& common, const SDeviceParams& _params);
    /// \brief Start devices: Run
    RequestResult execStart(const SCommonParams& common, const SDeviceParams& _params);
    /// \brief Stop devices: Stop
    RequestResult execStop(const SCommonParams& common, const SDeviceParams& _params);
    /// \brief Reset devices: ResetTask->ResetDevice
    RequestResult execReset(const SCommonParams& common, const SDeviceParams& _params);
    /// \brief Terminate devices: End
    RequestResult execTerminate(const SCommonParams& common, const SDeviceParams& _params);

    // Generic requests

    /// \brief Status request
    StatusRequestResult execStatus(const SStatusParams& _params);

  private:
    std::map<std::string, std::unique_ptr<SessionInfo>> m_sessions; ///< Map of partition ID to session info
    std::mutex mSessionsMtx; ///< Mutex of sessions map
    std::chrono::seconds m_timeout{ 30 }; ///< Request timeout in sec
    CDDSSubmit m_submit;                  ///< ODC to DDS submit resource converter
    CPluginManager m_triggers;            ///< Request triggers
    std::string m_restoreId;              ///< Restore ID

    void execRequestTrigger(const std::string& _plugin, const SCommonParams& common);
    void updateRestore();

    RequestResult createRequestResult(const SCommonParams& common,
                                      const SError& error,
                                      const std::string& msg,
                                      size_t execTime,
                                      AggregatedTopologyState aggregatedState,
                                      std::unique_ptr<TopologyState> fullState = nullptr);
    bool createDDSSession(const SCommonParams& common, SError& error);
    bool attachToDDSSession(const SCommonParams& common, SError& error, const std::string& _sessionID);
    bool submitDDSAgents(const SCommonParams& common, SError& error, const CDDSSubmit::SParams& _params);
    bool activateDDSTopology(const SCommonParams& common, SError& error, const std::string& _topologyFile, dds::tools_api::STopologyRequest::request_t::EUpdateType _updateType);
    bool waitForNumActiveAgents(const SCommonParams& common, SError& error, size_t _numAgents);
    bool requestCommanderInfo(const SCommonParams& common, SError& error, dds::tools_api::SCommanderInfoRequest::response_t& _commanderInfo);
    bool shutdownDDSSession(const SCommonParams& common, SError& error);
    bool resetFairMQTopo(const SCommonParams& common);
    bool createFairMQTopo(const SCommonParams& common, SError& error, const std::string& _topologyFile);
    bool createTopo(const SCommonParams& common, SError& error, const std::string& _topologyFile);
    bool setProperties(const SCommonParams& common, SError& error, const SSetPropertiesParams& _params);
    bool changeState(const SCommonParams& common,
                     SError& error,
                     TopologyTransition _transition,
                     const std::string& _path,
                     AggregatedTopologyState& aggregatedState,
                     TopologyState* _topologyState = nullptr);
    bool getState(const SCommonParams& common, SError& error, const std::string& _path, AggregatedTopologyState& aggregatedState, TopologyState* _topologyState = nullptr);
    bool changeStateConfigure(const SCommonParams& common, SError& error, const std::string& path, AggregatedTopologyState& aggregatedState, TopologyState* topologyState = nullptr);
    bool changeStateReset(const SCommonParams& common, SError& error, const std::string& path, AggregatedTopologyState& aggregatedState, TopologyState* topologyState = nullptr);

    void updateTopologyOnFailure();

    void fillError(const SCommonParams& common, SError& error, ErrorCode errorCode, const std::string& msg);
    void fillFatalError(const SCommonParams& common, SError& error, ErrorCode errorCode, const std::string& msg);

    AggregatedTopologyState aggregateStateForPath(const dds::topology_api::CTopology* _topo, const FairMQTopologyState& _fairmq, const std::string& _path);
    void fairMQToODCTopologyState(const dds::topology_api::CTopology* _topo, const FairMQTopologyState& _fairmq, TopologyState* _odc);

    SessionInfo& getOrCreateSessionInfo(const SCommonParams& common);

    SError checkSessionIsRunning(const SCommonParams& common, ErrorCode errorCode);

    std::string stateSummaryString(const SCommonParams& common, const FairMQTopologyState& _topologyState, DeviceState _expectedState, SessionInfo& sessionInfo);

    bool subscribeToDDSSession(const SCommonParams& common, SError& error);

    template<class Params_t>
    std::string topoFilepath(const SCommonParams& common, const Params_t& _params)
    {
        int count{ (_params.m_topologyFile.empty() ? 0 : 1) + (_params.m_topologyContent.empty() ? 0 : 1) + (_params.m_topologyScript.empty() ? 0 : 1) };
        if (count != 1) {
            throw std::runtime_error("Either topology filepath, content or script has to be set");
        }
        if (!_params.m_topologyFile.empty()) {
            return _params.m_topologyFile;
        }

        std::string content{ _params.m_topologyContent };

        // Execute topology script if needed
        if (!_params.m_topologyScript.empty()) {
            std::stringstream ssCmd;
            ssCmd << boost::process::search_path("bash").string() << " -c " << quoted(_params.m_topologyScript);
            const std::chrono::seconds timeout{ 30 };
            std::string out;
            std::string err;
            int exitCode{ EXIT_SUCCESS };
            std::string cmd{ ssCmd.str() };
            OLOG(info, common) << "Executing topology script " << std::quoted(cmd);
            execute(cmd, timeout, &out, &err, &exitCode);

            if (exitCode != EXIT_SUCCESS) {
                throw std::runtime_error(toString("Topology generation script ", quoted(cmd), " failed with exit code: ", exitCode, "; stderr: ", quoted(err), "; stdout: ", quoted(out)));
            }

            const std::string sout{ out.substr(0, std::min(out.length(), size_t(20))) };
            OLOG(info, common) << "Topology script executed successfully: stdout (" << quoted(sout) << "...) stderr (" << quoted(err) << ")";

            content = out;
        }

        // Create temp topology file with `content`
        const boost::filesystem::path tmpPath{ boost::filesystem::temp_directory_path() / boost::filesystem::unique_path() };
        boost::filesystem::create_directories(tmpPath);
        const boost::filesystem::path filepath{ tmpPath / "topology.xml" };
        std::ofstream file(filepath.string());
        if (!file.is_open()) {
            throw std::runtime_error(toString("Failed to create temp topology file ", quoted(filepath.string())));
        }
        file << content;
        OLOG(info, common) << "Temp topology file " << quoted(filepath.string()) << " created successfully";
        return filepath.string();
    }

    std::chrono::seconds requestTimeout(const SCommonParams& common) const;
};
} // namespace odc::core

#endif /* defined(__ODC__ControlService__) */
