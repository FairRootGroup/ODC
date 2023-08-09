/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_CONTROLLER
#define ODC_CORE_CONTROLLER

#include <odc/DDSSubmit.h>
#include <odc/Params.h>
#include <odc/Session.h>
#include <odc/Topology.h>

#include <dds/Tools.h>
#include <dds/Topology.h>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>

namespace odc::core
{

class Controller
{
  public:
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

    /// \brief Restore sessions for the specified ID
    ///  The function has to be called before the service start accepting request.
    /// \param [in] id ID of the restore file
    /// \param [in] dir directory to store restore files in
    void restore(const std::string& id, const std::string& dir);

    /// \brief Set directory where history file is stored
    /// \param [in] dir directory path
    void setHistoryDir(const std::string& dir) { mHistoryDir = dir; }

    /// \brief Set zone configs
    /// \param [in] zonesStr string representations of zone configs: "<name>:<cfgFilePath>:<envFilePath>"
    void setZoneCfgs(const std::vector<std::string>& zonesStr);

    /// \brief Set resource management system to be used by DDS
    /// \param [in] rms name of the RMS
    void setRMS(const std::string& rms) { mRMS = rms; }

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

    static void extractRequirements(const CommonParams& common, Session& session);

    struct Partition {
      std::unique_ptr<Session> session;
      std::unique_ptr<Topology> topology;
    };

  private:
    std::map<std::string, std::unique_ptr<Session>> mSessions; ///< Map of partition ID to session info
    std::mutex mSessionsMtx;                                   ///< Mutex of sessions map
    std::chrono::seconds mTimeout{ 30 };                       ///< Request timeout in sec
    DDSSubmit mSubmit;                                         ///< ODC to DDS submit resource converter
    std::string mRestoreId;                                    ///< Restore ID
    std::string mRestoreDir;                                   ///< Restore file directory
    std::string mHistoryDir;                                   ///< History file directory
    std::map<std::string, ZoneConfig> mZoneCfgs;               ///< stores zones configuration (cfgFilePath/envFilePath) by zone name
    std::string mRMS{ "localhost" };                           ///< resource management system to be used by DDS

    void updateRestore();
    void updateHistory(const CommonParams& common, const std::string& sessionId);

    std::unordered_set<std::string> submit(const CommonParams& common, Session& session, Error& error, const std::string& plugin, const std::string& res, bool extractResources);
    void activate(const CommonParams& common, Session& session, Error& error);

    bool createDDSSession(           const CommonParams& common, Session& session, Error& error);
    bool attachToDDSSession(         const CommonParams& common, Session& session, Error& error, const std::string& sessionID);
    bool shutdownDDSSession(         const CommonParams& common, Session& session, Error& error);
    std::string getActiveDDSTopology(const CommonParams& common, Session& session, Error& error);

    bool submitDDSAgents(      const CommonParams& common, Session& session, Error& error, const DDSSubmitParams& params);
    bool waitForNumActiveSlots(const CommonParams& common, Session& session, Error& error, size_t numSlots);
    void ShutdownDDSAgent(     const CommonParams& common, Session& session, uint64_t agentID);

    bool activateDDSTopology(const CommonParams& common, Session& session, Error& error, dds::tools_api::STopologyRequest::request_t::EUpdateType updateType);
    bool createDDSTopology(  const CommonParams& common, Session& session, Error& error);

    bool createTopology(const CommonParams& common, Session& session, Error& error);
    bool resetTopology(Session& session);

    bool changeState(         const CommonParams& common, Session& session, Error& error, const std::string& path, TopoTransition transition, TopologyState& topologyState);
    bool changeStateConfigure(const CommonParams& common, Session& session, Error& error, const std::string& path, TopologyState& topologyState);
    bool changeStateReset(    const CommonParams& common, Session& session, Error& error, const std::string& path, TopologyState& topologyState);
    bool waitForState(        const CommonParams& common, Session& session, Error& error, const std::string& path, DeviceState expState);
    bool setProperties(       const CommonParams& common, Session& session, Error& error, const std::string& path, const SetPropertiesParams::Props& props, TopologyState& topologyState);
    void getState(            const CommonParams& common, Session& session, Error& error, const std::string& path, TopologyState& state);

    void fillAndLogError(               const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void fillAndLogFatalError(          const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void fillAndLogFatalErrorLineByLine(const CommonParams& common, Error& error, ErrorCode errorCode, const std::string& msg);
    void logFatalLineByLine(            const CommonParams& common, const std::string& msg);

    RequestResult createRequestResult(const CommonParams& common, const Session& session, const Error& error, const std::string& msg, TopologyState&& topologyState, const std::unordered_set<std::string>& hosts);
    RequestResult createRequestResult(const CommonParams& common, const std::string& sessionId, const Error& error, const std::string& msg, TopologyState&& topologyState, const std::unordered_set<std::string>& hosts);
    AggregatedState aggregateStateForPath(const dds::topology_api::CTopology* ddsTopo, const TopoState& topoState, const std::string& path);

    Session& acquireSession(const CommonParams& common);
    void removeSession(const CommonParams& common);

    void stateSummaryOnFailure(const CommonParams& common, Session& session, const TopoState& topoState, DeviceState expectedState);
    void attemptSubmitRecovery(const CommonParams& common, Session& session, Error& error, const std::vector<DDSSubmitParams>& ddsParams, const std::map<std::string, uint32_t>& agentCounts);
    void updateTopology(const CommonParams& common, Session& session);

    std::string topoFilepath(const CommonParams& common, const std::string& topologyFile, const std::string& topologyContent, const std::string& topologyScript);

    std::chrono::seconds requestTimeout(const CommonParams& common) const
    {
        std::chrono::seconds configuredTimeoutS = (common.mTimeout == 0 ? mTimeout : std::chrono::seconds(common.mTimeout));
        std::chrono::milliseconds configuredTimeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(configuredTimeoutS);
        // subtract time elapsed since the beginning of the request
        std::chrono::milliseconds realTimeoutMs = configuredTimeoutMs - common.mTimer.duration();
        OLOG(debug, common) << "Configured request timeout: " << configuredTimeoutMs.count() << "ms "
            << (common.mTimeout == 0 ? "(controller default)" : "(request parameter)")
            << ", remaining time: " << realTimeoutMs.count() << "ms";
        return std::chrono::duration_cast<std::chrono::seconds>(realTimeoutMs);
    }

    uint32_t getNumSlots(const CommonParams& common, Session& session) const;
    dds::tools_api::SAgentInfoRequest::responseVector_t getAgentInfo(const CommonParams& common, Session& session) const;

    void printStateStats(const CommonParams& common, const TopoState& topoState, bool debugLog = false);
};

} // namespace odc::core

#endif /* defined(ODC_CORE_CONTROLLER) */
