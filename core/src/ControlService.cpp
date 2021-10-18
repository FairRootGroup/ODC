// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "ControlService.h"
#include "DDSSubmit.h"
#include "Error.h"
#include "Logger.h"
#include "Process.h"
#include "Restore.h"
#include "TimeMeasure.h"
#include "Topology.h"
// DDS
#include <dds/Tools.h>
#include <dds/Topology.h>
// BOOST
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
// STD
#include <algorithm>

using namespace odc;
using namespace odc::core;
using namespace std;
using namespace dds;
using namespace dds::tools_api;
using namespace dds::topology_api;
namespace bfs = boost::filesystem;
namespace bp = boost::process;

//
// CControlService::SImpl
//
struct CControlService::SImpl
{
    using DDSTopologyPtr_t = std::shared_ptr<dds::topology_api::CTopology>;
    using DDSSessionPtr_t = std::shared_ptr<dds::tools_api::CSession>;
    using FairMQTopologyPtr_t = std::shared_ptr<Topology>;

    struct SSessionInfo
    {
        using Ptr_t = std::shared_ptr<SSessionInfo>;
        using Map_t = std::map<partitionID_t, Ptr_t>;

        DDSTopologyPtr_t m_topo{ nullptr };              ///< DDS topology
        DDSSessionPtr_t m_session{ nullptr };            ///< DDS session
        FairMQTopologyPtr_t m_fairmqTopology{ nullptr }; ///< FairMQ topology
        partitionID_t m_partitionID;                     ///< External partition ID of this DDS session
    };

    SImpl()
    {
        //    fair::Logger::SetConsoleSeverity("debug");
    }

    ~SImpl()
    {
    }

    void setTimeout(const chrono::seconds& _timeout)
    {
        m_timeout = _timeout;
    }

    void registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap);
    void registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap);
    void execRequestTrigger(const string& _plugin, const SCommonParams& _common);

    void restore(const std::string& _id);
    void updateRestore();

    // Core API calls
    SReturnValue execInitialize(const SCommonParams& _common, const SInitializeParams& _params);
    SReturnValue execSubmit(const SCommonParams& _common, const SSubmitParams& _params);
    SReturnValue execActivate(const SCommonParams& _common, const SActivateParams& _params);
    SReturnValue execRun(const SCommonParams& _common,
                         const SInitializeParams& _initializeParams,
                         const SSubmitParams& _submitParams,
                         const SActivateParams& _activateParams);
    SReturnValue execUpdate(const SCommonParams& _common, const SUpdateParams& _params);
    SReturnValue execShutdown(const SCommonParams& _common);

    SReturnValue execSetProperties(const SCommonParams& _common, const SSetPropertiesParams& _params);
    SReturnValue execGetState(const SCommonParams& _common, const SDeviceParams& _params);

    SReturnValue execConfigure(const SCommonParams& _common, const SDeviceParams& _params);
    SReturnValue execStart(const SCommonParams& _common, const SDeviceParams& _params);
    SReturnValue execStop(const SCommonParams& _common, const SDeviceParams& _params);
    SReturnValue execReset(const SCommonParams& _common, const SDeviceParams& _params);
    SReturnValue execTerminate(const SCommonParams& _common, const SDeviceParams& _params);

    SStatusReturnValue execStatus(const SStatusParams& _params);

  private:
    SReturnValue createReturnValue(const SCommonParams& _common,
                                   const SError& _error,
                                   const std::string& _msg,
                                   size_t _execTime,
                                   AggregatedTopologyState _aggregatedState,
                                   SReturnDetails::ptr_t _details = nullptr);
    bool createDDSSession(const SCommonParams& _common, SError& _error);
    bool attachToDDSSession(const SCommonParams& _common, SError& _error, const std::string& _sessionID);
    bool submitDDSAgents(const SCommonParams& _common, SError& _error, const CDDSSubmit::SParams& _params);
    bool activateDDSTopology(const SCommonParams& _common,
                             SError& _error,
                             const std::string& _topologyFile,
                             dds::tools_api::STopologyRequest::request_t::EUpdateType _updateType);
    bool waitForNumActiveAgents(const SCommonParams& _common, SError& _error, size_t _numAgents);
    bool requestCommanderInfo(const SCommonParams& _common,
                              SError& _error,
                              SCommanderInfoRequest::response_t& _commanderInfo);
    bool shutdownDDSSession(const SCommonParams& _common, SError& _error);
    bool resetFairMQTopo(const SCommonParams& _common);
    bool createFairMQTopo(const SCommonParams& _common, SError& _error, const std::string& _topologyFile);
    bool createTopo(const SCommonParams& _common, SError& _error, const std::string& _topologyFile);
    bool setProperties(const SCommonParams& _common, SError& _error, const SSetPropertiesParams& _params);
    bool changeState(const SCommonParams& _common,
                     SError& _error,
                     TopologyTransition _transition,
                     const std::string& _path,
                     AggregatedTopologyState& _aggregatedState,
                     TopologyState* _topologyState = nullptr);
    bool getState(const SCommonParams& _common,
                  SError& _error,
                  const string& _path,
                  AggregatedTopologyState& _aggregatedState,
                  TopologyState* _topologyState = nullptr);
    bool changeStateConfigure(const SCommonParams& _common,
                              SError& _error,
                              const std::string& _path,
                              AggregatedTopologyState& _aggregatedState,
                              TopologyState* _topologyState = nullptr);
    bool changeStateReset(const SCommonParams& _common,
                          SError& _error,
                          const std::string& _path,
                          AggregatedTopologyState& _aggregatedState,
                          TopologyState* _topologyState = nullptr);

    void fillError(const SCommonParams& _common, SError& _error, ErrorCode _errorCode, const string& _msg);

    AggregatedTopologyState aggregateStateForPath(const DDSTopologyPtr_t& _topo,
                                                  const FairMQTopologyState& _fairmq,
                                                  const string& _path);
    void fairMQToODCTopologyState(const DDSTopologyPtr_t& _topo,
                                  const FairMQTopologyState& _fairmq,
                                  TopologyState* _odc);

    SSessionInfo::Ptr_t getOrCreateSessionInfo(const SCommonParams& _common);

    SError checkSessionIsRunning(const SCommonParams& _common, ErrorCode _errorCode);

    string stateSummaryString(const SCommonParams& _common,
                              const FairMQTopologyState& _topologyState,
                              DeviceState _expectedState,
                              DDSTopologyPtr_t _topo);

    bool subscribeToDDSSession(const SCommonParams& _common, SError& _error);

    template <class Params_t>
    string topoFilepath(const SCommonParams& _common, const Params_t& _params);

    chrono::seconds requestTimeout(const SCommonParams& _common) const;

    // Disable copy constructors and assignment operators
    SImpl(const SImpl&) = delete;
    SImpl(SImpl&&) = delete;
    SImpl& operator=(const SImpl&) = delete;
    SImpl& operator=(SImpl&&) = delete;

    SSessionInfo::Map_t m_sessions;                                    ///< Map of partition ID to session info
    mutex m_sessionsMutex;                                             ///< Mutex of sessions map
    chrono::seconds m_timeout{ 30 };                                   ///< Request timeout in sec
    CDDSSubmit::Ptr_t m_submit{ make_shared<CDDSSubmit>() };           ///< ODC to DDS submit resource converter
    CPluginManager::Ptr_t m_triggers{ make_shared<CPluginManager>() }; ///< Request triggers
    string m_restoreId;                                                ///< Restore ID
};

void CControlService::SImpl::registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap)
{
    for (const auto& v : _pluginMap)
    {
        m_submit->registerPlugin(v.first, v.second);
    }
}

void CControlService::SImpl::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap)
{
    const set<string> avail{ "Initialize", "Submit", "Activate", "Run",   "Update",    "Configure", "SetProperties",
                             "GetState",   "Start",  "Stop",     "Reset", "Terminate", "Shutdown",  "Status" };
    for (const auto& v : _triggerMap)
    {
        if (avail.count(v.first) == 0)
        {
            throw runtime_error(toString("Failed to add request trigger ",
                                         quoted(v.first),
                                         ". Invalid request name. Valid names are: ",
                                         quoted(boost::algorithm::join(avail, ", "))));
        }
        m_triggers->registerPlugin(v.first, v.second);
    }
}

void CControlService::SImpl::execRequestTrigger(const string& _plugin, const SCommonParams& _common)
{
    if (m_triggers->isPluginRegistered(_plugin))
    {
        try
        {
            OLOG(ESeverity::debug, _common) << "Executing request trigger " << quoted(_plugin);
            string out{ m_triggers->execPlugin(_plugin, "", _common.m_partitionID, _common.m_runNr) };
            OLOG(ESeverity::debug, _common) << "Request trigger " << quoted(_plugin) << " done: " << out;
        }
        catch (exception& _e)
        {
            OLOG(ESeverity::error, _common) << "Request trigger " << quoted(_plugin) << " failed: " << _e.what();
        }
    }
    else
    {
        OLOG(ESeverity::debug, _common) << "No plugins registered for " << quoted(_plugin);
    }
}

void CControlService::SImpl::restore(const std::string& _id)
{
    m_restoreId = _id;

    OLOG(ESeverity::debug) << "Restoring sessions for " << quoted(_id);
    auto data{ CRestoreFile(_id).read() };
    for (const auto& v : data.m_partitions)
    {
        OLOG(ESeverity::debug, v.m_partitionId, 0)
            << "Restoring (" << quoted(v.m_partitionId) << "/" << quoted(v.m_sessionId) << ")";
        auto result{ execInitialize(SCommonParams(v.m_partitionId, 0, 0), SInitializeParams(v.m_sessionId)) };
        if (result.m_error.m_code)
        {
            OLOG(ESeverity::debug, v.m_partitionId, 0)
                << "Failed to attach to the session. Executing Shutdown trigger for (" << quoted(v.m_partitionId) << "/"
                << quoted(v.m_sessionId) << ")";
            execRequestTrigger("Shutdown", SCommonParams(v.m_partitionId, 0, 0));
        }
        else
        {
            OLOG(ESeverity::debug, v.m_partitionId, 0)
                << "Successfully attached to the session (" << quoted(v.m_partitionId) << "/" << quoted(v.m_sessionId)
                << ")";
        }
    }
}

void CControlService::SImpl::updateRestore()
{
    if (m_restoreId.empty())
        return;

    OLOG(ESeverity::debug) << "Updating restore file " << quoted(m_restoreId) << "...";
    SRestoreData data;

    lock_guard<mutex> lock(m_sessionsMutex);
    for (const auto& v : m_sessions)
    {
        const auto& info{ v.second };
        // TODO: Add running sessions only?
        // if (info->m_session->IsRunning()) // Note: IsRunning() throws
        data.m_partitions.push_back(SRestorePartition(info->m_partitionID, to_string(info->m_session->getSessionID())));
    }

    // Write the the file is locked by m_sessionsMutex
    // This is done in order to prevent write failure in case of a parallel execution.
    CRestoreFile(m_restoreId, data).write();
}

SReturnValue CControlService::SImpl::execInitialize(const SCommonParams& _common, const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error;
    if (_params.m_sessionID.empty())
    {
        // Shutdown DDS session if it is running already
        // Create new DDS session
        shutdownDDSSession(_common, error) && createDDSSession(_common, error) && subscribeToDDSSession(_common, error);
    }
    else
    {
        // Shutdown DDS session if it is running already
        // Attach to an existing DDS session
        bool success{ shutdownDDSSession(_common, error) && attachToDDSSession(_common, error, _params.m_sessionID) &&
                      subscribeToDDSSession(_common, error) };

        // Request current active topology, if any
        // If topology is active, create DDS and FairMQ topology
        if (success)
        {
            SCommanderInfoRequest::response_t commanderInfo;
            requestCommanderInfo(_common, error, commanderInfo) && !commanderInfo.m_activeTopologyPath.empty() &&
                createTopo(_common, error, commanderInfo.m_activeTopologyPath) &&
                createFairMQTopo(_common, error, commanderInfo.m_activeTopologyPath);
        }
    }
    execRequestTrigger("Initialize", _common);
    updateRestore();
    return createReturnValue(_common, error, "Initialize done", measure.duration(), AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execSubmit(const SCommonParams& _common, const SSubmitParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error{ checkSessionIsRunning(_common, ErrorCode::DDSSubmitAgentsFailed) };

    // Get DDS submit parameters from ODC resource plugin
    CDDSSubmit::SParams ddsParams;
    if (!error.m_code)
    {
        try
        {
            ddsParams =
                m_submit->makeParams(_params.m_plugin, _params.m_resources, _common.m_partitionID, _common.m_runNr);
        }
        catch (exception& _e)
        {
            fillError(_common, error, ErrorCode::ResourcePluginFailed, toString("Resource plugin failed: ", _e.what()));
        }
    }

    if (!error.m_code)
    {
        const size_t requiredSlots{ ddsParams.m_requiredNumSlots };
        // Submit DDS agents
        // Wait until all agents are active
        submitDDSAgents(_common, error, ddsParams) && waitForNumActiveAgents(_common, error, requiredSlots);
    }
    execRequestTrigger("Submit", _common);
    return createReturnValue(_common, error, "Submit done", measure.duration(), AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execActivate(const SCommonParams& _common, const SActivateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Activate DDS topology
    // Create Topology
    SError error{ checkSessionIsRunning(_common, ErrorCode::DDSActivateTopologyFailed) };
    if (!error.m_code)
    {
        string topo;
        try
        {
            topo = topoFilepath(_common, _params);
        }
        catch (exception& _e)
        {
            fillError(_common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", _e.what()));
        }
        if (!error.m_code)
        {
            activateDDSTopology(_common, error, topo, STopologyRequest::request_t::EUpdateType::ACTIVATE) &&
                createTopo(_common, error, topo) && createFairMQTopo(_common, error, topo);
        }
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle : AggregatedTopologyState::Undefined };
    execRequestTrigger("Activate", _common);
    return createReturnValue(_common, error, "Activate done", measure.duration(), state);
}

SReturnValue CControlService::SImpl::execRun(const SCommonParams& _common,
                                             const SInitializeParams& _initializeParams,
                                             const SSubmitParams& _submitParams,
                                             const SActivateParams& _activateParams)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Run request doesn't support attachment to a DDS session.
    // Execute consecuently Initialize, Submit and Activate.
    SError error;
    if (!_initializeParams.m_sessionID.empty())
    {
        error = SError(MakeErrorCode(ErrorCode::RequestNotSupported), "Attachment to a DDS session not supported");
    }
    else
    {
        error = execInitialize(_common, _initializeParams).m_error;
        if (!error.m_code)
        {
            error = execSubmit(_common, _submitParams).m_error;
            if (!error.m_code)
            {
                error = execActivate(_common, _activateParams).m_error;
            }
        }
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle : AggregatedTopologyState::Undefined };
    execRequestTrigger("Run", _common);
    return createReturnValue(_common, error, "Run done", measure.duration(), state);
}

SReturnValue CControlService::SImpl::execUpdate(const SCommonParams& _common, const SUpdateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    // Reset devices' state
    // Update DDS topology
    // Create Topology
    // Configure devices' state
    SError error;

    string topo;
    try
    {
        topo = topoFilepath(_common, _params);
    }
    catch (exception& _e)
    {
        fillError(_common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", _e.what()));
    }

    if (!error.m_code)
    {
        changeStateReset(_common, error, "", state) && resetFairMQTopo(_common) &&
            activateDDSTopology(_common, error, topo, STopologyRequest::request_t::EUpdateType::UPDATE) &&
            createTopo(_common, error, topo) && createFairMQTopo(_common, error, topo) &&
            changeStateConfigure(_common, error, "", state);
    }
    execRequestTrigger("Update", _common);
    return createReturnValue(_common, error, "Update done", measure.duration(), state);
}

SReturnValue CControlService::SImpl::execShutdown(const SCommonParams& _common)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    shutdownDDSSession(_common, error);
    execRequestTrigger("Shutdown", _common);
    return createReturnValue(_common, error, "Shutdown done", measure.duration(), AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execSetProperties(const SCommonParams& _common,
                                                       const SSetPropertiesParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    setProperties(_common, error, _params);
    execRequestTrigger("SetProperties", _common);
    return createReturnValue(
        _common, error, "SetProperties done", measure.duration(), AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execGetState(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    getState(_common, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    execRequestTrigger("GetState", _common);
    return createReturnValue(_common, error, "GetState done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execConfigure(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeStateConfigure(
        _common, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    execRequestTrigger("Configure", _common);
    return createReturnValue(_common, error, "Configure done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execStart(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_common,
                error,
                TopologyTransition::Run,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    execRequestTrigger("Start", _common);
    return createReturnValue(_common, error, "Start done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execStop(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_common,
                error,
                TopologyTransition::Stop,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    execRequestTrigger("Stop", _common);
    return createReturnValue(_common, error, "Stop done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execReset(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeStateReset(
        _common, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    execRequestTrigger("Reset", _common);
    return createReturnValue(_common, error, "Reset done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execTerminate(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_common,
                error,
                TopologyTransition::End,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    execRequestTrigger("Terminate", _common);
    return createReturnValue(_common, error, "Terminate done", measure.duration(), state, details);
}

SStatusReturnValue CControlService::SImpl::execStatus(const SStatusParams& _params)
{
    lock_guard<mutex> lock(m_sessionsMutex);
    STimeMeasure<std::chrono::milliseconds> measure;
    SStatusReturnValue result;
    for (const auto& v : m_sessions)
    {
        const auto& info{ v.second };
        SPartitionStatus status;
        status.m_partitionID = info->m_partitionID;
        try
        {
            status.m_sessionID = to_string(info->m_session->getSessionID());
            status.m_sessionStatus = (info->m_session->IsRunning()) ? ESessionStatus::running : ESessionStatus::stopped;
        }
        catch (exception& _e)
        {
            OLOG(ESeverity::warning, status.m_partitionID, 0)
                << "Failed to get session ID or session status: " << _e.what();
        }

        // Filter running sessions if needed
        if ((_params.m_running && status.m_sessionStatus == ESessionStatus::running) || (!_params.m_running))
        {
            try
            {
                status.m_aggregatedState =
                    (info->m_fairmqTopology != nullptr && info->m_topo != nullptr)
                        ? aggregateStateForPath(info->m_topo, info->m_fairmqTopology->GetCurrentState(), "")
                        : AggregatedTopologyState::Undefined;
            }
            catch (exception& _e)
            {
                OLOG(ESeverity::warning, status.m_partitionID, 0) << "Failed to get an aggregated state: " << _e.what();
            }
            result.m_partitions.push_back(status);
        }
    }
    result.m_statusCode = EStatusCode::ok;
    result.m_msg = "Status done";
    result.m_execTime = measure.duration();
    execRequestTrigger("Status", SCommonParams());
    return result;
}

SReturnValue CControlService::SImpl::createReturnValue(const SCommonParams& _common,
                                                       const SError& _error,
                                                       const std::string& _msg,
                                                       size_t _execTime,
                                                       AggregatedTopologyState _aggregatedState,
                                                       SReturnDetails::ptr_t _details)
{
    OLOG(ESeverity::debug, _common) << "Creating return value...";
    auto info{ getOrCreateSessionInfo(_common) };
    string sidStr{ to_string(info->m_session->getSessionID()) };
    EStatusCode status{ _error.m_code ? EStatusCode::error : EStatusCode::ok };
    return SReturnValue(
        status, _msg, _execTime, _error, _common.m_partitionID, _common.m_runNr, sidStr, _aggregatedState, _details);
}

bool CControlService::SImpl::createDDSSession(const SCommonParams& _common, SError& _error)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_common) };
        boost::uuids::uuid sessionID{ info->m_session->create() };
        OLOG(ESeverity::info, _common) << "DDS session created with session ID: " << to_string(sessionID);
    }
    catch (exception& _e)
    {
        fillError(_common,
                  _error,
                  ErrorCode::DDSCreateSessionFailed,
                  toString("Failed to create a DDS session: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::SImpl::attachToDDSSession(const SCommonParams& _common,
                                                SError& _error,
                                                const std::string& _sessionID)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_session->attach(_sessionID);
        OLOG(ESeverity::info, _common) << "Attach to a DDS session with session ID: " << _sessionID;
    }
    catch (exception& _e)
    {
        fillError(_common,
                  _error,
                  ErrorCode::DDSAttachToSessionFailed,
                  toString("Failed to attach to a DDS session: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::SImpl::submitDDSAgents(const SCommonParams& _common,
                                             SError& _error,
                                             const CDDSSubmit::SParams& _params)
{
    bool success(true);

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_rms = _params.m_rmsPlugin;
    requestInfo.m_instances = _params.m_numAgents;
    requestInfo.m_slots = _params.m_numSlots;
    requestInfo.m_config = _params.m_configFile;

    std::condition_variable cv;

    SSubmitRequest::ptr_t requestPtr = SSubmitRequest::makeRequest(requestInfo);

    requestPtr->setMessageCallback(
        [&success, &_error, &_common, this](const SMessageResponseData& _message)
        {
            if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
            {
                success = false;
                fillError(
                    _common, _error, ErrorCode::DDSSubmitAgentsFailed, toString("Sumbit error: ", _message.m_msg));
            }
            else
            {
                OLOG(ESeverity::debug, _common) << "Submit: " << _message.m_msg;
            }
        });

    requestPtr->setDoneCallback(
        [&cv, &_common]()
        {
            OLOG(ESeverity::info, _common) << "Agent submission done";
            cv.notify_all();
        });

    auto info{ getOrCreateSessionInfo(_common) };
    info->m_session->sendRequest<SSubmitRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, requestTimeout(_common));

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        fillError(_common, _error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
    }
    else
    {
        OLOG(ESeverity::info, _common) << "Agent submission done successfully";
    }
    return success;
}

bool CControlService::SImpl::requestCommanderInfo(const SCommonParams& _common,
                                                  SError& _error,
                                                  SCommanderInfoRequest::response_t& _commanderInfo)
{
    try
    {
        stringstream ss;
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_session->syncSendRequest<SCommanderInfoRequest>(
            SCommanderInfoRequest::request_t(), _commanderInfo, requestTimeout(_common), &ss);
        OLOG(ESeverity::info, _common) << ss.str();
        OLOG(ESeverity::debug, _common) << "Commander info: " << _commanderInfo;
        return true;
    }
    catch (exception& _e)
    {
        fillError(_common,
                  _error,
                  ErrorCode::DDSCommanderInfoFailed,
                  toString("Error getting DDS commander info: ", _e.what()));
        return false;
    }
}

bool CControlService::SImpl::waitForNumActiveAgents(const SCommonParams& _common, SError& _error, size_t _numAgents)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_session->waitForNumAgents<CSession::EAgentState::active>(_numAgents, requestTimeout(_common));
    }
    catch (std::exception& _e)
    {
        fillError(_common, _error, ErrorCode::RequestTimeout, toString("Timeout waiting for DDS agents: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::SImpl::activateDDSTopology(const SCommonParams& _common,
                                                 SError& _error,
                                                 const string& _topologyFile,
                                                 STopologyRequest::request_t::EUpdateType _updateType)
{
    bool success(true);

    STopologyRequest::request_t topoInfo;
    topoInfo.m_topologyFile = _topologyFile;
    topoInfo.m_disableValidation = true;
    topoInfo.m_updateType = _updateType;

    std::condition_variable cv;

    STopologyRequest::ptr_t requestPtr{ STopologyRequest::makeRequest(topoInfo) };

    requestPtr->setMessageCallback(
        [&success, &_error, &_common, this](const SMessageResponseData& _message)
        {
            if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
            {
                success = false;
                fillError(_common,
                          _error,
                          ErrorCode::DDSActivateTopologyFailed,
                          toString("Activate error: ", _message.m_msg));
            }
            else
            {
                OLOG(ESeverity::debug, _common) << "Activate: " << _message.m_msg;
            }
        });

    requestPtr->setProgressCallback(
        [&_common](const SProgressResponseData& _progress)
        {
            uint32_t completed{ _progress.m_completed + _progress.m_errors };
            if (completed == _progress.m_total)
            {
                OLOG(ESeverity::info, _common) << "Activated tasks (" << _progress.m_completed << "), errors ("
                                               << _progress.m_errors << "), total (" << _progress.m_total << ")";
            }
        });

    requestPtr->setDoneCallback([&cv]() { cv.notify_all(); });

    auto info{ getOrCreateSessionInfo(_common) };
    info->m_session->sendRequest<STopologyRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus{ cv.wait_for(lock, requestTimeout(_common)) };

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        fillError(_common, _error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
        OLOG(ESeverity::error, _common) << _error;
    }
    else
    {
        OLOG(ESeverity::info, _common) << "Topology " << quoted(_topologyFile) << " activated successfully";
    }
    return success;
}

bool CControlService::SImpl::shutdownDDSSession(const SCommonParams& _common, SError& _error)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_topo.reset();
        info->m_fairmqTopology.reset();
        // We stop the session anyway if session ID is not nil.
        // Session can already be stopped by `dds-session stop` but session ID is not yet reset to nil.
        // If session is already stopped CSession::shutdown will reset pointers.
        if (info->m_session->getSessionID() != boost::uuids::nil_uuid())
        {
            info->m_session->shutdown();
            if (info->m_session->getSessionID() == boost::uuids::nil_uuid())
            {
                OLOG(ESeverity::info, _common) << "DDS session shutted down";
            }
            else
            {
                fillError(_common, _error, ErrorCode::DDSShutdownSessionFailed, "Failed to shut down DDS session");
                return false;
            }
        }
    }
    catch (exception& _e)
    {
        fillError(_common, _error, ErrorCode::DDSShutdownSessionFailed, toString("Shutdown failed: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::SImpl::createTopo(const SCommonParams& _common, SError& _error, const std::string& _topologyFile)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_topo = make_shared<dds::topology_api::CTopology>(_topologyFile);
        OLOG(ESeverity::info, _common) << "DDS topology " << std::quoted(_topologyFile) << " created successfully";
    }
    catch (exception& _e)
    {
        fillError(_common,
                  _error,
                  ErrorCode::DDSCreateTopologyFailed,
                  toString("Failed to initialize DDS topology: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::SImpl::resetFairMQTopo(const SCommonParams& _common)
{
    auto info{ getOrCreateSessionInfo(_common) };
    info->m_fairmqTopology.reset();
    return true;
}

bool CControlService::SImpl::createFairMQTopo(const SCommonParams& _common,
                                              SError& _error,
                                              const std::string& _topologyFile)
{
    auto info{ getOrCreateSessionInfo(_common) };
    try
    {
        info->m_fairmqTopology.reset();
        info->m_fairmqTopology = make_shared<Topology>(dds::topology_api::CTopology(_topologyFile), info->m_session);
    }
    catch (exception& _e)
    {
        info->m_fairmqTopology = nullptr;
        fillError(_common,
                  _error,
                  ErrorCode::FairMQCreateTopologyFailed,
                  toString("Failed to initialize FairMQ topology: ", _e.what()));
    }
    return info->m_fairmqTopology != nullptr;
}

bool CControlService::SImpl::changeState(const SCommonParams& _common,
                                         SError& _error,
                                         TopologyTransition _transition,
                                         const string& _path,
                                         AggregatedTopologyState& _aggregatedState,
                                         TopologyState* _topologyState)
{
    auto info{ getOrCreateSessionInfo(_common) };
    if (info->m_fairmqTopology == nullptr)
    {
        fillError(_common, _error, ErrorCode::FairMQChangeStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    auto it{ expectedState.find(_transition) };
    DeviceState _expectedState{ it != expectedState.end() ? it->second : DeviceState::Undefined };
    if (_expectedState == DeviceState::Undefined)
    {
        fillError(_common,
                  _error,
                  ErrorCode::FairMQChangeStateFailed,
                  toString("Unexpected FairMQ transition ", _transition));
        return false;
    }

    bool success(true);

    try
    {
        auto result{ info->m_fairmqTopology->ChangeState(_transition, _path, requestTimeout(_common)) };

        success = !result.first;
        if (success)
        {
            try
            {
                _aggregatedState = AggregateState(result.second);
            }
            catch (exception& _e)
            {
                success = false;
                fillError(_common,
                          _error,
                          ErrorCode::FairMQChangeStateFailed,
                          toString("Aggregate topology state failed: ", _e.what()));
                OLOG(ESeverity::debug, _common)
                    << stateSummaryString(_common, result.second, _expectedState, info->m_topo);
            }
            if (_topologyState != nullptr)
                fairMQToODCTopologyState(info->m_topo, result.second, _topologyState);
        }
        else
        {
            switch (static_cast<ErrorCode>(result.first.value()))
            {
                case ErrorCode::OperationTimeout:
                    fillError(_common,
                              _error,
                              ErrorCode::RequestTimeout,
                              toString("Timed out waiting for change state ", _transition));
                    break;
                default:
                    fillError(_common,
                              _error,
                              ErrorCode::FairMQChangeStateFailed,
                              toString("FairMQ change state failed: ", result.first.message()));
                    break;
            }
            OLOG(ESeverity::debug, _common)
                << stateSummaryString(_common, info->m_fairmqTopology->GetCurrentState(), _expectedState, info->m_topo);
        }
    }
    catch (exception& _e)
    {
        success = false;
        fillError(_common, _error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", _e.what()));
        OLOG(ESeverity::debug, _common) << stateSummaryString(
            _common, info->m_fairmqTopology->GetCurrentState(), _expectedState, info->m_topo);
    }

    if (success)
    {
        OLOG(ESeverity::info, _common) << "State changed to " << _aggregatedState << " via " << _transition
                                       << " transition";
    }

    return success;
}

bool CControlService::SImpl::changeStateConfigure(const SCommonParams& _common,
                                                  SError& _error,
                                                  const string& _path,
                                                  AggregatedTopologyState& _aggregatedState,
                                                  TopologyState* _topologyState)
{
    return changeState(_common, _error, TopologyTransition::InitDevice, _path, _aggregatedState, _topologyState) &&
           changeState(_common, _error, TopologyTransition::CompleteInit, _path, _aggregatedState, _topologyState) &&
           changeState(_common, _error, TopologyTransition::Bind, _path, _aggregatedState, _topologyState) &&
           changeState(_common, _error, TopologyTransition::Connect, _path, _aggregatedState, _topologyState) &&
           changeState(_common, _error, TopologyTransition::InitTask, _path, _aggregatedState, _topologyState);
}

bool CControlService::SImpl::changeStateReset(const SCommonParams& _common,
                                              SError& _error,
                                              const string& _path,
                                              AggregatedTopologyState& _aggregatedState,
                                              TopologyState* _topologyState)
{
    return changeState(_common, _error, TopologyTransition::ResetTask, _path, _aggregatedState, _topologyState) &&
           changeState(_common, _error, TopologyTransition::ResetDevice, _path, _aggregatedState, _topologyState);
}

bool CControlService::SImpl::getState(const SCommonParams& _common,
                                      SError& _error,
                                      const string& _path,
                                      AggregatedTopologyState& _aggregatedState,
                                      TopologyState* _topologyState)
{
    auto info{ getOrCreateSessionInfo(_common) };
    if (info->m_fairmqTopology == nullptr)
    {
        fillError(_common, _error, ErrorCode::FairMQGetStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    bool success(true);
    auto const state(info->m_fairmqTopology->GetCurrentState());

    try
    {
        _aggregatedState = aggregateStateForPath(info->m_topo, state, _path);
    }
    catch (exception& _e)
    {
        success = false;
        fillError(_common, _error, ErrorCode::FairMQGetStateFailed, toString("Get state failed: ", _e.what()));
    }
    if (_topologyState != nullptr)
        fairMQToODCTopologyState(info->m_topo, state, _topologyState);

    return success;
}

bool CControlService::SImpl::setProperties(const SCommonParams& _common,
                                           SError& _error,
                                           const SSetPropertiesParams& _params)
{
    auto info{ getOrCreateSessionInfo(_common) };
    if (info->m_fairmqTopology == nullptr)
    {
        fillError(_common, _error, ErrorCode::FairMQSetPropertiesFailed, "FairMQ topology is not initialized");
        return false;
    }

    bool success(true);

    try
    {
        auto result{ info->m_fairmqTopology->SetProperties(
            _params.m_properties, _params.m_path, requestTimeout(_common)) };

        success = !result.first;
        if (success)
        {
            OLOG(ESeverity::info, _common) << "Set property finished successfully";
        }
        else
        {
            switch (static_cast<ErrorCode>(result.first.value()))
            {
                case ErrorCode::OperationTimeout:
                    fillError(_common,
                              _error,
                              ErrorCode::RequestTimeout,
                              toString("Timed out waiting for set property: ", result.first.message()));
                    break;
                default:
                    fillError(_common,
                              _error,
                              ErrorCode::FairMQSetPropertiesFailed,
                              toString("Set property error message: ", result.first.message()));
                    break;
            }
            stringstream ss;
            size_t count{ 0 };
            ss << "List of failed devices for SetProperties request (" << result.second.size() << "): " << endl;
            for (auto taskId : result.second)
            {
                ss << right << setw(7) << count << " Task: ID (" << taskId << ")";
                try
                {
                    if (info->m_topo != nullptr)
                    {
                        auto task{ info->m_topo->getRuntimeTaskById(taskId) };
                        ss << ", task path (" << task.m_taskPath << ")" << endl;
                    }
                }
                catch (const exception& _e)
                {
                    OLOG(ESeverity::error, _common)
                        << "Failed to get task with ID (" << taskId << ") from topology (" << info->m_topo->getName()
                        << ") at filepath " << std::quoted(info->m_topo->getFilepath()) << ". Error: " << _e.what();
                }
                count++;
            }
            OLOG(ESeverity::debug, _common) << ss.str();
        }
    }
    catch (exception& _e)
    {
        success = false;
        fillError(_common, _error, ErrorCode::FairMQSetPropertiesFailed, toString("Set property failed: ", _e.what()));
    }

    return success;
}

AggregatedTopologyState CControlService::SImpl::aggregateStateForPath(const DDSTopologyPtr_t& _topo,
                                                                      const FairMQTopologyState& _topoState,
                                                                      const string& _path)
{
    if (_path.empty())
        return AggregateState(_topoState);

    if (_topo == nullptr)
        throw runtime_error("DDS topology is not initialized");

    try
    {
        // Check if path points to a single task
        // If task is found than return it's state as an aggregated topology state

        // Throws if task not found for path
        const auto& task{ _topo->getRuntimeTask(_path) };
        auto it{ find_if(_topoState.cbegin(),
                         _topoState.cend(),
                         [&](const FairMQTopologyState::value_type& _v) { return _v.taskId == task.m_taskId; }) };
        if (it != _topoState.cend())
            return static_cast<AggregatedTopologyState>(it->state);

        throw runtime_error("Device not found for path " + _path);
    }
    catch (exception& _e)
    {
        // In case of exception check that path contains multiple tasks

        // Collect all task IDs to a set for fast search
        set<Id_t> taskIds;
        auto it{ _topo->getRuntimeTaskIteratorMatchingPath(_path) };
        for_each(it.first,
                 it.second,
                 [&](const STopoRuntimeTask::FilterIterator_t::value_type& _v) { taskIds.insert(_v.second.m_taskId); });

        if (taskIds.empty())
            throw runtime_error("No tasks found matching the path " + _path);

        // Find a state of a first task
        auto firstIt{ find_if(_topoState.cbegin(),
                              _topoState.cend(),
                              [&](const FairMQTopologyState::value_type& _v)
                              { return _v.taskId == *(taskIds.begin()); }) };
        if (firstIt == _topoState.cend())
            throw runtime_error("No states found for path " + _path);

        // Check that all selected devices have the same state
        AggregatedTopologyState first{ static_cast<AggregatedTopologyState>(firstIt->state) };
        if (std::all_of(_topoState.cbegin(),
                        _topoState.cend(),
                        [&](const FairMQTopologyState::value_type& _v)
                        { return (taskIds.count(_v.taskId) > 0) ? _v.state == first : true; }))
        {
            return first;
        }

        return AggregatedTopologyState::Mixed;
    }
}

void CControlService::SImpl::fairMQToODCTopologyState(const DDSTopologyPtr_t& _topo,
                                                      const FairMQTopologyState& _fairmq,
                                                      TopologyState* _odc)
{
    if (_odc == nullptr || _topo == nullptr)
        return;

    _odc->reserve(_fairmq.size());
    for (const auto& state : _fairmq)
    {
        const auto& task = _topo->getRuntimeTaskById(state.taskId);
        _odc->push_back(SDeviceStatus(state, task.m_taskPath));
    }
}

void CControlService::SImpl::fillError(const SCommonParams& _common,
                                       SError& _error,
                                       ErrorCode _errorCode,
                                       const string& _msg)
{
    _error.m_code = MakeErrorCode(_errorCode);
    _error.m_details = _msg;
    OLOG(ESeverity::error, _common) << _error;
}

CControlService::SImpl::SSessionInfo::Ptr_t CControlService::SImpl::getOrCreateSessionInfo(const SCommonParams& _common)
{
    const auto& prt{ _common.m_partitionID };
    lock_guard<mutex> lock(m_sessionsMutex);
    auto it{ m_sessions.find(prt) };
    if (it == m_sessions.end())
    {
        auto newSessionInfo{ make_shared<SSessionInfo>() };
        newSessionInfo->m_session = make_shared<CSession>();
        newSessionInfo->m_partitionID = prt;
        m_sessions.insert(pair<partitionID_t, SSessionInfo::Ptr_t>(prt, newSessionInfo));
        OLOG(ESeverity::debug, _common) << "Return new session info";
        return newSessionInfo;
    }
    OLOG(ESeverity::debug, _common) << "Return existing session info";
    return it->second;
}

SError CControlService::SImpl::checkSessionIsRunning(const SCommonParams& _common, ErrorCode _errorCode)
{
    SError error;
    auto info{ getOrCreateSessionInfo(_common) };
    if (!info->m_session->IsRunning())
    {
        fillError(_common, error, _errorCode, "DDS session is not running. Use Init or Run to start the session.");
    }
    return error;
}

string CControlService::SImpl::stateSummaryString(const SCommonParams& _common,
                                                  const FairMQTopologyState& _topologyState,
                                                  DeviceState _expectedState,
                                                  DDSTopologyPtr_t _topo)
{
    size_t taskTotalCount{ _topologyState.size() };
    size_t taskFailedCount{ 0 };
    stringstream ss;
    for (const auto& status : _topologyState)
    {
        // Print only failed devices
        if (status.state == _expectedState)
            continue;

        taskFailedCount++;
        if (taskFailedCount == 1)
        {
            ss << "List of failed devices for an expected state " << _expectedState << ":";
        }
        ss << endl
           << right << setw(7) << taskFailedCount << " Device: state (" << status.state << "), last state ("
           << status.lastState << "), task ID (" << status.taskId << "), collection ID (" << status.collectionId
           << "), "
           << "subscribed (" << status.subscribed_to_state_changes << ")";
        try
        {
            if (_topo != nullptr)
            {
                auto task{ _topo->getRuntimeTaskById(status.taskId) };
                ss << ", task path (" << task.m_taskPath << ")";
            }
        }
        catch (const exception& _e)
        {
            OLOG(ESeverity::error, _common)
                << "Failed to get task with ID (" << status.taskId << ") from topology (" << _topo->getName()
                << ") at filepath " << std::quoted(_topo->getFilepath()) << ". Error: " << _e.what();
        }
    }

    auto collectionMap{ GroupByCollectionId(_topologyState) };
    size_t collectionTotalCount{ collectionMap.size() };
    size_t collectionFailedCount{ 0 };
    for (const auto& states : collectionMap)
    {
        auto collectionState{ AggregateState(states.second) };
        auto collectionId{ states.first };
        // Print only failed collections
        if (collectionState == _expectedState)
            continue;

        collectionFailedCount++;
        if (collectionFailedCount == 1)
        {
            ss << endl << "List of failed collections for an expected state " << _expectedState << ":";
        }
        ss << endl
           << right << setw(7) << collectionFailedCount << " Collection: state (" << collectionState
           << "), collection ID (" << collectionId << ")";

        try
        {
            if (_topo != nullptr)
            {
                auto collection{ _topo->getRuntimeCollectionById(collectionId) };
                ss << ", collection path (" << collection.m_collectionPath << "), number of tasks ("
                   << collection.m_collection->getNofTasks() << ")";
            }
        }
        catch (const exception& _e)
        {
            OLOG(ESeverity::error, _common)
                << "Failed to get collection with ID (" << collectionId << ") from topology (" << _topo->getName()
                << ") at filepath " << std::quoted(_topo->getFilepath()) << ". Error: " << _e.what();
        }
    }

    size_t taskSuccessCount{ taskTotalCount - taskFailedCount };
    size_t collectionSuccessCount{ collectionTotalCount - collectionFailedCount };
    ss << endl
       << "Summary for expected state (" << _expectedState << "): " << endl
       << "   Tasks total/success/failed (" << taskTotalCount << "/" << taskSuccessCount << "/" << taskFailedCount
       << ")" << endl
       << "   Collections total/success/failed (" << collectionTotalCount << "/" << collectionSuccessCount << "/"
       << collectionFailedCount << ")";

    return ss.str();
}

bool CControlService::SImpl::subscribeToDDSSession(const SCommonParams& _common, SError& _error)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_common) };
        if (info->m_session->IsRunning())
        {
            // Subscrube on TaskDone events
            auto request{ SOnTaskDoneRequest::makeRequest(SOnTaskDoneRequest::request_t()) };
            request->setResponseCallback(
                [_common](const SOnTaskDoneResponseData& _info)
                {
                    ESeverity severity{ (_info.m_exitCode != 0 || _info.m_signal != 0) ? ESeverity::fatal
                                                                                       : ESeverity::debug };
                    OLOG(severity, _common)
                        << "Task (" << _info.m_taskID << ") with path (" << _info.m_taskPath << ") exited with code ("
                        << _info.m_exitCode << ") and signal (" << _info.m_signal << ") on (" << _info.m_host
                        << ") in working directory (" << _info.m_wrkDir << ")";
                });
            info->m_session->sendRequest<SOnTaskDoneRequest>(request);
            OLOG(ESeverity::info, _common)
                << "Subscribed to task done event from session " << quoted(to_string(info->m_session->getSessionID()));
        }
        else
        {
            fillError(_common,
                      _error,
                      ErrorCode::DDSSubscribeToSessionFailed,
                      "Failed to subscribe to task done events: session is not running");
            return false;
        }
    }
    catch (exception& _e)
    {
        fillError(_common,
                  _error,
                  ErrorCode::DDSSubscribeToSessionFailed,
                  string("Failed to subscribe to task done events: ") + _e.what());
        return false;
    }
    return true;
}

template <class Params_t>
string CControlService::SImpl::topoFilepath(const SCommonParams& _common, const Params_t& _params)
{
    int count{ (_params.m_topologyFile.empty() ? 0 : 1) + (_params.m_topologyContent.empty() ? 0 : 1) +
               (_params.m_topologyScript.empty() ? 0 : 1) };
    if (count != 1)
    {
        throw runtime_error("Either topology filepath, content or script has to be set");
    }
    if (!_params.m_topologyFile.empty())
        return _params.m_topologyFile;

    string content{ _params.m_topologyContent };

    // Execute topology script if needed
    if (!_params.m_topologyScript.empty())
    {
        stringstream ssCmd;
        ssCmd << bp::search_path("bash").string() << " -c " << quoted(_params.m_topologyScript);
        const std::chrono::seconds timeout{ 30 };
        string out;
        string err;
        int exitCode{ EXIT_SUCCESS };
        string cmd{ ssCmd.str() };
        OLOG(ESeverity::info, _common) << "Executing topology script " << std::quoted(cmd);
        execute(cmd, timeout, &out, &err, &exitCode);

        if (exitCode != EXIT_SUCCESS)
        {
            throw runtime_error(toString("Topology script ",
                                         quoted(cmd),
                                         " execution failed with exit code: ",
                                         exitCode,
                                         "; error message: ",
                                         err));
        }

        const string sout{ out.substr(0, min(out.length(), size_t(20))) };
        OLOG(ESeverity::info, _common) << "Topology script executed successfully: stdout (" << quoted(sout)
                                       << "...) stderr (" << quoted(err) << ")";

        content = out;
    }

    // Create temp topology file with `content`
    const bfs::path tmpPath{ bfs::temp_directory_path() / bfs::unique_path() };
    bfs::create_directories(tmpPath);
    const bfs::path filepath{ tmpPath / "topology.xml" };
    ofstream f(filepath.string());
    if (!f.is_open())
    {
        throw runtime_error(toString("Failed to create temp topology file ", quoted(filepath.string())));
    }
    f << content;
    OLOG(ESeverity::info, _common) << "Temp topology file " << quoted(filepath.string()) << " created successfully";
    return filepath.string();
}

chrono::seconds CControlService::SImpl::requestTimeout(const SCommonParams& _common) const
{
    auto timeout{ (_common.m_timeout == 0) ? m_timeout : chrono::seconds(_common.m_timeout) };
    OLOG(ESeverity::debug, _common) << "Request timeout: " << timeout.count() << " sec";
    return timeout;
}

//
// Misc
//

namespace odc::core
{
    std::ostream& operator<<(std::ostream& _os, const SError& _error)
    {
        return _os << _error.m_code << " (" << _error.m_details << ")";
    }

    std::ostream& operator<<(std::ostream& _os, const SCommonParams& _params)
    {
        return _os << "CommonParams: partitionID=" << quoted(_params.m_partitionID) << ", runNr=" << _params.m_runNr
                   << ", timeout=" << _params.m_timeout;
    }

    std::ostream& operator<<(std::ostream& _os, const SInitializeParams& _params)
    {
        return _os << "InitilizeParams: sid=" << quoted(_params.m_sessionID);
    }

    std::ostream& operator<<(std::ostream& _os, const SSubmitParams& _params)
    {
        return _os << "SubmitParams: plugin=" << quoted(_params.m_plugin)
                   << "; resources=" << quoted(_params.m_resources);
    }

    std::ostream& operator<<(std::ostream& _os, const SActivateParams& _params)
    {
        return _os << "ActivateParams: topologyFile=" << quoted(_params.m_topologyFile)
                   << "; topologyContent=" << quoted(_params.m_topologyContent)
                   << "; topologyScript=" << quoted(_params.m_topologyScript);
    }

    std::ostream& operator<<(std::ostream& _os, const SUpdateParams& _params)
    {
        return _os << "UpdateParams: topologyFile=" << quoted(_params.m_topologyFile)
                   << "; topologyContent=" << quoted(_params.m_topologyContent)
                   << "; topologyScript=" << quoted(_params.m_topologyScript);
    }

    std::ostream& operator<<(std::ostream& _os, const SSetPropertiesParams& _params)
    {
        _os << "SetPropertiesParams: path=" << quoted(_params.m_path) << "; properties={";
        for (const auto& v : _params.m_properties)
        {
            _os << " (" << v.first << ":" << v.second << ") ";
        }
        return _os << "}";
    }

    std::ostream& operator<<(std::ostream& _os, const SDeviceParams& _params)
    {
        return _os << "DeviceParams: path=" << quoted(_params.m_path) << "; detailed=" << _params.m_detailed;
    }

    std::ostream& operator<<(std::ostream& _os, const SStatusParams& _params)
    {
        return _os << "StatusParams: running=" << _params.m_running;
    }
} // namespace odc::core

//
// CControlService
//

CControlService::CControlService()
    : m_impl(make_shared<CControlService::SImpl>())
{
}

void CControlService::setTimeout(const chrono::seconds& _timeout)
{
    m_impl->setTimeout(_timeout);
}

void CControlService::registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap)
{
    m_impl->registerResourcePlugins(_pluginMap);
}

void CControlService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap)
{
    m_impl->registerRequestTriggers(_triggerMap);
}

void CControlService::restore(const std::string& _id)
{
    m_impl->restore(_id);
}

SReturnValue CControlService::execInitialize(const SCommonParams& _common, const SInitializeParams& _params)
{
    return m_impl->execInitialize(_common, _params);
}

SReturnValue CControlService::execSubmit(const SCommonParams& _common, const SSubmitParams& _params)
{
    return m_impl->execSubmit(_common, _params);
}

SReturnValue CControlService::execActivate(const SCommonParams& _common, const SActivateParams& _params)
{
    return m_impl->execActivate(_common, _params);
}

SReturnValue CControlService::execRun(const SCommonParams& _common,
                                      const SInitializeParams& _initializeParams,
                                      const SSubmitParams& _submitParams,
                                      const SActivateParams& _activateParams)
{
    return m_impl->execRun(_common, _initializeParams, _submitParams, _activateParams);
}

SReturnValue CControlService::execUpdate(const SCommonParams& _common, const SUpdateParams& _params)
{
    return m_impl->execUpdate(_common, _params);
}

SReturnValue CControlService::execShutdown(const SCommonParams& _common)
{
    return m_impl->execShutdown(_common);
}

SReturnValue CControlService::execSetProperties(const SCommonParams& _common, const SSetPropertiesParams& _params)
{
    return m_impl->execSetProperties(_common, _params);
}

SReturnValue CControlService::execGetState(const SCommonParams& _common, const SDeviceParams& _params)
{
    return m_impl->execGetState(_common, _params);
}

SReturnValue CControlService::execConfigure(const SCommonParams& _common, const SDeviceParams& _params)
{
    return m_impl->execConfigure(_common, _params);
}

SReturnValue CControlService::execStart(const SCommonParams& _common, const SDeviceParams& _params)
{
    return m_impl->execStart(_common, _params);
}

SReturnValue CControlService::execStop(const SCommonParams& _common, const SDeviceParams& _params)
{
    return m_impl->execStop(_common, _params);
}

SReturnValue CControlService::execReset(const SCommonParams& _common, const SDeviceParams& _params)
{
    return m_impl->execReset(_common, _params);
}

SReturnValue CControlService::execTerminate(const SCommonParams& _common, const SDeviceParams& _params)
{
    return m_impl->execTerminate(_common, _params);
}

SStatusReturnValue CControlService::execStatus(const SStatusParams& _params)
{
    return m_impl->execStatus(_params);
}
