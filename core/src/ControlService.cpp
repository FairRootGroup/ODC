// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "ControlService.h"
#include "DDSSubmit.h"
#include "Error.h"
#include "Logger.h"
#include "TimeMeasure.h"
#include "Topology.h"
// DDS
#include <dds/Tools.h>
#include <dds/Topology.h>

using namespace odc;
using namespace odc::core;
using namespace std;
using namespace dds;
using namespace dds::tools_api;
using namespace dds::topology_api;

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

    // Core API calls
    SReturnValue execInitialize(const partitionID_t& _partitionID, const SInitializeParams& _params);
    SReturnValue execSubmit(const partitionID_t& _partitionID, const SSubmitParams& _params);
    SReturnValue execActivate(const partitionID_t& _partitionID, const SActivateParams& _params);
    SReturnValue execRun(const partitionID_t& _partitionID,
                         const SInitializeParams& _initializeParams,
                         const SSubmitParams& _submitParams,
                         const SActivateParams& _activateParams);
    SReturnValue execUpdate(const partitionID_t& _partitionID, const SUpdateParams& _params);
    SReturnValue execShutdown(const partitionID_t& _partitionID);

    SReturnValue execSetProperties(const partitionID_t& _partitionID, const SSetPropertiesParams& _params);
    SReturnValue execGetState(const partitionID_t& _partitionID, const SDeviceParams& _params);

    SReturnValue execConfigure(const partitionID_t& _partitionID, const SDeviceParams& _params);
    SReturnValue execStart(const partitionID_t& _partitionID, const SDeviceParams& _params);
    SReturnValue execStop(const partitionID_t& _partitionID, const SDeviceParams& _params);
    SReturnValue execReset(const partitionID_t& _partitionID, const SDeviceParams& _params);
    SReturnValue execTerminate(const partitionID_t& _partitionID, const SDeviceParams& _params);

    SStatusReturnValue execStatus(const SStatusParams& _params);

  private:
    SReturnValue createReturnValue(const partitionID_t& _partitionID,
                                   const SError& _error,
                                   const std::string& _msg,
                                   size_t _execTime,
                                   AggregatedTopologyState _aggregatedState,
                                   SReturnDetails::ptr_t _details = nullptr);
    bool createDDSSession(const partitionID_t& _partitionID, SError& _error);
    bool attachToDDSSession(const partitionID_t& _partitionID, SError& _error, const std::string& _sessionID);
    bool submitDDSAgents(const partitionID_t& _partitionID, SError& _error, const CDDSSubmit::SParams& _params);
    bool activateDDSTopology(const partitionID_t& _partitionID,
                             SError& _error,
                             const std::string& _topologyFile,
                             dds::tools_api::STopologyRequest::request_t::EUpdateType _updateType);
    bool waitForNumActiveAgents(const partitionID_t& _partitionID, SError& _error, size_t _numAgents);
    bool requestCommanderInfo(const partitionID_t& _partitionID,
                              SError& _error,
                              SCommanderInfoRequest::response_t& _commanderInfo);
    bool shutdownDDSSession(const partitionID_t& _partitionID, SError& _error);
    bool resetFairMQTopo(const partitionID_t& _partitionID);
    bool createFairMQTopo(const partitionID_t& _partitionID, SError& _error, const std::string& _topologyFile);
    bool createTopo(const partitionID_t& _partitionID, SError& _error, const std::string& _topologyFile);
    bool setProperties(const partitionID_t& _partitionID, SError& _error, const SSetPropertiesParams& _params);
    bool changeState(const partitionID_t& _partitionID,
                     SError& _error,
                     TopologyTransition _transition,
                     const std::string& _path,
                     AggregatedTopologyState& _aggregatedState,
                     TopologyState* _topologyState = nullptr);
    bool getState(const partitionID_t& _partitionID,
                  SError& _error,
                  const string& _path,
                  AggregatedTopologyState& _aggregatedState,
                  TopologyState* _topologyState = nullptr);
    bool changeStateConfigure(const partitionID_t& _partitionID,
                              SError& _error,
                              const std::string& _path,
                              AggregatedTopologyState& _aggregatedState,
                              TopologyState* _topologyState = nullptr);
    bool changeStateReset(const partitionID_t& _partitionID,
                          SError& _error,
                          const std::string& _path,
                          AggregatedTopologyState& _aggregatedState,
                          TopologyState* _topologyState = nullptr);

    void fillError(SError& _error, ErrorCode _errorCode, const string& _msg);

    AggregatedTopologyState aggregateStateForPath(const DDSTopologyPtr_t& _topo,
                                                                 const FairMQTopologyState& _fairmq,
                                                                 const string& _path);
    void fairMQToODCTopologyState(const DDSTopologyPtr_t& _topo,
                                  const FairMQTopologyState& _fairmq,
                                  TopologyState* _odc);

    SSessionInfo::Ptr_t getOrCreateSessionInfo(const partitionID_t& _partitionID);

    SError checkSessionIsRunning(const partitionID_t& _partitionID, ErrorCode _errorCode);

    string stateSummaryString(const FairMQTopologyState& _topologyState,
                              DeviceState _expectedState,
                              DDSTopologyPtr_t _topo);

    bool subscribeToDDSSession(const partitionID_t& _partitionID, SError& _error);

    // Disable copy constructors and assignment operators
    SImpl(const SImpl&) = delete;
    SImpl(SImpl&&) = delete;
    SImpl& operator=(const SImpl&) = delete;
    SImpl& operator=(SImpl&&) = delete;

    SSessionInfo::Map_t m_sessions;                          ///< Map of partition ID to session info
    chrono::seconds m_timeout{ 30 };                         ///< Request timeout in sec
    CDDSSubmit::Ptr_t m_submit{ make_shared<CDDSSubmit>() }; ///< ODC to DDS submit resource converter
};

void CControlService::SImpl::registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap)
{
    for (const auto& v : _pluginMap)
    {
        m_submit->registerPlugin(v.first, v.second);
    }
}

SReturnValue CControlService::SImpl::execInitialize(const partitionID_t& _partitionID, const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error;
    if (_params.m_sessionID.empty())
    {
        // Shutdown DDS session if it is running already
        // Create new DDS session
        shutdownDDSSession(_partitionID, error) && createDDSSession(_partitionID, error) &&
            subscribeToDDSSession(_partitionID, error);
    }
    else
    {
        // Shutdown DDS session if it is running already
        // Attach to an existing DDS session
        bool success{ shutdownDDSSession(_partitionID, error) &&
                      attachToDDSSession(_partitionID, error, _params.m_sessionID) &&
                      subscribeToDDSSession(_partitionID, error) };

        // Request current active topology, if any
        // If topology is active, create DDS and FairMQ topology
        if (success)
        {
            SCommanderInfoRequest::response_t commanderInfo;
            requestCommanderInfo(_partitionID, error, commanderInfo) && !commanderInfo.m_activeTopologyPath.empty() &&
                createTopo(_partitionID, error, commanderInfo.m_activeTopologyPath) &&
                createFairMQTopo(_partitionID, error, commanderInfo.m_activeTopologyPath);
        }
    }
    return createReturnValue(
        _partitionID, error, "Initialize done", measure.duration(), AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execSubmit(const partitionID_t& _partitionID, const SSubmitParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error{ checkSessionIsRunning(_partitionID, ErrorCode::DDSSubmitAgentsFailed) };

    // Get DDS submit parameters from ODC resource plugin
    CDDSSubmit::SParams ddsParams;
    if (!error.m_code)
    {
        try
        {
            ddsParams = m_submit->makeParams(_params.m_plugin, _params.m_resources);
        }
        catch (exception& _e)
        {
            fillError(error, ErrorCode::ResourcePluginFailed, string("Resource plugin failed: ") + _e.what());
        }
    }

    if (!error.m_code)
    {
        const size_t requiredSlots{ ddsParams.m_requiredNumSlots };
        // Submit DDS agents
        // Wait until all agents are active
        submitDDSAgents(_partitionID, error, ddsParams) && waitForNumActiveAgents(_partitionID, error, requiredSlots);
    }

    return createReturnValue(
        _partitionID, error, "Submit done", measure.duration(), AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execActivate(const partitionID_t& _partitionID, const SActivateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Activate DDS topology
    // Create Topology
    SError error{ checkSessionIsRunning(_partitionID, ErrorCode::DDSActivateTopologyFailed) };
    if (!error.m_code)
    {
        activateDDSTopology(
            _partitionID, error, _params.m_topologyFile, STopologyRequest::request_t::EUpdateType::ACTIVATE) &&
            createTopo(_partitionID, error, _params.m_topologyFile) &&
            createFairMQTopo(_partitionID, error, _params.m_topologyFile);
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle
                                                 : AggregatedTopologyState::Undefined };
    return createReturnValue(_partitionID, error, "Activate done", measure.duration(), state);
}

SReturnValue CControlService::SImpl::execRun(const partitionID_t& _partitionID,
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
        error = execInitialize(_partitionID, _initializeParams).m_error;
        if (!error.m_code)
        {
            error = execSubmit(_partitionID, _submitParams).m_error;
            if (!error.m_code)
            {
                error = execActivate(_partitionID, _activateParams).m_error;
            }
        }
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle
                                                 : AggregatedTopologyState::Undefined };
    return createReturnValue(_partitionID, error, "Run done", measure.duration(), state);
}

SReturnValue CControlService::SImpl::execUpdate(const partitionID_t& _partitionID, const SUpdateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    // Reset devices' state
    // Update DDS topology
    // Create Topology
    // Configure devices' state
    SError error;
    changeStateReset(_partitionID, error, "", state) && resetFairMQTopo(_partitionID) &&
        activateDDSTopology(
            _partitionID, error, _params.m_topologyFile, STopologyRequest::request_t::EUpdateType::UPDATE) &&
        createTopo(_partitionID, error, _params.m_topologyFile) &&
        createFairMQTopo(_partitionID, error, _params.m_topologyFile) &&
        changeStateConfigure(_partitionID, error, "", state);
    return createReturnValue(_partitionID, error, "Update done", measure.duration(), state);
}

SReturnValue CControlService::SImpl::execShutdown(const partitionID_t& _partitionID)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    shutdownDDSSession(_partitionID, error);
    return createReturnValue(
        _partitionID, error, "Shutdown done", measure.duration(), AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execSetProperties(const partitionID_t& _partitionID,
                                                       const SSetPropertiesParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    setProperties(_partitionID, error, _params);
    return createReturnValue(_partitionID,
                             error,
                             "SetProperties done",
                             measure.duration(),
                             AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execGetState(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    getState(_partitionID, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "GetState done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execConfigure(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeStateConfigure(
        _partitionID, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "ConfigureRun done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execStart(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_partitionID,
                error,
                TopologyTransition::Run,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Start done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execStop(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_partitionID,
                error,
                TopologyTransition::Stop,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Stop done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execReset(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeStateReset(
        _partitionID, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Reset done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execTerminate(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_partitionID,
                error,
                TopologyTransition::End,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Terminate done", measure.duration(), state, details);
}

SStatusReturnValue CControlService::SImpl::execStatus(const SStatusParams& /* _params */)
{
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
            OLOG(ESeverity::warning) << "Failed to get session ID or session status of " << quoted(status.m_partitionID)
                                     << " partition: " << _e.what();
        }
        try
        {
            status.m_aggregatedState =
                (info->m_fairmqTopology != nullptr && info->m_topo != nullptr)
                    ? aggregateStateForPath(info->m_topo, info->m_fairmqTopology->GetCurrentState(), "")
                    : AggregatedTopologyState::Undefined;
        }
        catch (exception& _e)
        {
            OLOG(ESeverity::warning) << "Failed to get an aggregated state of " << quoted(status.m_partitionID)
                                     << " partition: " << _e.what();
        }
        result.m_partitions.push_back(status);
    }
    result.m_statusCode = EStatusCode::ok;
    result.m_msg = "Status done";
    result.m_execTime = measure.duration();
    return result;
}

SReturnValue CControlService::SImpl::createReturnValue(const partitionID_t& _partitionID,
                                                       const SError& _error,
                                                       const std::string& _msg,
                                                       size_t _execTime,
                                                       AggregatedTopologyState _aggregatedState,
                                                       SReturnDetails::ptr_t _details)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    string sidStr{ to_string(info->m_session->getSessionID()) };
    EStatusCode status{ _error.m_code ? EStatusCode::error : EStatusCode::ok };
    return SReturnValue(status, _msg, _execTime, _error, _partitionID, sidStr, _aggregatedState, _details);
}

bool CControlService::SImpl::createDDSSession(const partitionID_t& _partitionID, SError& _error)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_partitionID) };
        boost::uuids::uuid sessionID{ info->m_session->create() };
        OLOG(ESeverity::info) << "DDS session created with session ID: " << to_string(sessionID);
    }
    catch (exception& _e)
    {
        fillError(_error, ErrorCode::DDSCreateSessionFailed, string("Failed to create a DDS session: ") + _e.what());
        return false;
    }
    return true;
}

bool CControlService::SImpl::attachToDDSSession(const partitionID_t& _partitionID,
                                                SError& _error,
                                                const std::string& _sessionID)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_partitionID) };
        info->m_session->attach(_sessionID);
        OLOG(ESeverity::info) << "Attach to a DDS session with session ID: " << _sessionID;
    }
    catch (exception& _e)
    {
        fillError(
            _error, ErrorCode::DDSAttachToSessionFailed, string("Failed to attach to a DDS session: ") + _e.what());
        return false;
    }
    return true;
}

bool CControlService::SImpl::submitDDSAgents(const partitionID_t& _partitionID,
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
        [&success, &_error, this](const SMessageResponseData& _message)
        {
            if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
            {
                success = false;
                fillError(_error, ErrorCode::DDSSubmitAgentsFailed, string("Server reports error: ") + _message.m_msg);
            }
            else
            {
                OLOG(ESeverity::debug) << "Server reports: " << _message.m_msg;
            }
        });

    requestPtr->setDoneCallback(
        [&cv]()
        {
            OLOG(ESeverity::info) << "Agent submission done";
            cv.notify_all();
        });

    auto info{ getOrCreateSessionInfo(_partitionID) };
    info->m_session->sendRequest<SSubmitRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        fillError(_error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
    }
    else
    {
        OLOG(ESeverity::info) << "Agent submission done successfully";
    }
    return success;
}

bool CControlService::SImpl::requestCommanderInfo(const partitionID_t& _partitionID,
                                                  SError& _error,
                                                  SCommanderInfoRequest::response_t& _commanderInfo)
{
    try
    {
        stringstream ss;
        auto info{ getOrCreateSessionInfo(_partitionID) };
        info->m_session->syncSendRequest<SCommanderInfoRequest>(
            SCommanderInfoRequest::request_t(), _commanderInfo, m_timeout, &ss);
        OLOG(ESeverity::info) << ss.str();
        OLOG(ESeverity::debug) << "Commander info: " << _commanderInfo;
        return true;
    }
    catch (exception& _e)
    {
        fillError(_error, ErrorCode::DDSCommanderInfoFailed, string("Error getting DDS commander info: ") + _e.what());
        return false;
    }
}

bool CControlService::SImpl::waitForNumActiveAgents(const partitionID_t& _partitionID,
                                                    SError& _error,
                                                    size_t _numAgents)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_partitionID) };
        info->m_session->waitForNumAgents<CSession::EAgentState::active>(_numAgents, m_timeout);
    }
    catch (std::exception& _e)
    {
        fillError(_error, ErrorCode::RequestTimeout, string("Timeout waiting for DDS agents: ") + _e.what());
        return false;
    }
    return true;
}

bool CControlService::SImpl::activateDDSTopology(const partitionID_t& _partitionID,
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
        [&success, &_error, this](const SMessageResponseData& _message)
        {
            if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
            {
                success = false;
                fillError(
                    _error, ErrorCode::DDSActivateTopologyFailed, string("Server reports error: ") + _message.m_msg);
            }
            else
            {
                OLOG(ESeverity::debug) << "Server reports: " << _message.m_msg;
            }
        });

    requestPtr->setProgressCallback(
        [&_partitionID](const SProgressResponseData& _progress)
        {
            uint32_t completed{ _progress.m_completed + _progress.m_errors };
            if (completed == _progress.m_total)
            {
                OLOG(ESeverity::info) << "Partition " << quoted(_partitionID) << " containes activated tasks("
                                      << _progress.m_completed << "), errors (" << _progress.m_errors << "), total ("
                                      << _progress.m_total << ")";
            }
        });

    requestPtr->setDoneCallback([&cv]() { cv.notify_all(); });

    auto info{ getOrCreateSessionInfo(_partitionID) };
    info->m_session->sendRequest<STopologyRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus{ cv.wait_for(lock, m_timeout) };

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        fillError(_error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
        OLOG(ESeverity::error) << _error;
    }
    else
    {
        OLOG(ESeverity::info) << "Topology " << quoted(_topologyFile) << " for partition " << quoted(_partitionID)
                              << " activated successfully";
    }
    return success;
}

bool CControlService::SImpl::shutdownDDSSession(const partitionID_t& _partitionID, SError& _error)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_partitionID) };
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
                OLOG(ESeverity::info) << "DDS session shutted down";
            }
            else
            {
                fillError(_error, ErrorCode::DDSShutdownSessionFailed, "Failed to shut down DDS session");
                return false;
            }
        }
    }
    catch (exception& _e)
    {
        fillError(_error, ErrorCode::DDSShutdownSessionFailed, string("Shutdown failed: ") + _e.what());
        return false;
    }
    return true;
}

bool CControlService::SImpl::createTopo(const partitionID_t& _partitionID,
                                        SError& _error,
                                        const std::string& _topologyFile)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_partitionID) };
        info->m_topo = make_shared<dds::topology_api::CTopology>(_topologyFile);
        OLOG(ESeverity::info) << "DDS topology " << std::quoted(_topologyFile) << " for partition "
                              << std::quoted(_partitionID) << " created successfully";
    }
    catch (exception& _e)
    {
        fillError(
            _error, ErrorCode::DDSCreateTopologyFailed, string("Failed to initialize DDS topology: ") + _e.what());
        return false;
    }
    return true;
}

bool CControlService::SImpl::resetFairMQTopo(const partitionID_t& _partitionID)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    info->m_fairmqTopology.reset();
    return true;
}

bool CControlService::SImpl::createFairMQTopo(const partitionID_t& _partitionID,
                                              SError& _error,
                                              const std::string& _topologyFile)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    try
    {
        info->m_fairmqTopology.reset();
        info->m_fairmqTopology = make_shared<Topology>(dds::topology_api::CTopology(_topologyFile),
                                                       info->m_session);
    }
    catch (exception& _e)
    {
        info->m_fairmqTopology = nullptr;
        fillError(_error,
                  ErrorCode::FairMQCreateTopologyFailed,
                  string("Failed to initialize FairMQ topology: ") + _e.what());
    }
    return info->m_fairmqTopology != nullptr;
}

bool CControlService::SImpl::changeState(const partitionID_t& _partitionID,
                                         SError& _error,
                                         TopologyTransition _transition,
                                         const string& _path,
                                         AggregatedTopologyState& _aggregatedState,
                                         TopologyState* _topologyState)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    if (info->m_fairmqTopology == nullptr)
    {
        fillError(_error, ErrorCode::FairMQChangeStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    auto it{ expectedState.find(_transition) };
    DeviceState _expectedState{ it != expectedState.end() ? it->second : DeviceState::Undefined };
    if (_expectedState == DeviceState::Undefined)
    {
        fillError(_error,
                  ErrorCode::FairMQChangeStateFailed,
                  toString("Unexpected FairMQ transition ", _transition));
        return false;
    }

    bool success(true);

    try
    {
        std::condition_variable cv;

        info->m_fairmqTopology->AsyncChangeState(
            _transition,
            _path,
            m_timeout,
            [&cv, &success, &_aggregatedState, &_topologyState, &_error, &info, &_expectedState, this](
                std::error_code _ec, FairMQTopologyState _state)
            {
                success = !_ec;
                if (success)
                {
                    try
                    {
                        _aggregatedState = AggregateState(_state);
                    }
                    catch (exception& _e)
                    {
                        success = false;
                        fillError(_error,
                                  ErrorCode::FairMQChangeStateFailed,
                                  string("Aggregate topology state failed: ") + _e.what());
                        OLOG(ESeverity::error) << stateSummaryString(_state, _expectedState, info->m_topo);
                    }
                    if (_topologyState != nullptr)
                        fairMQToODCTopologyState(info->m_topo, _state, _topologyState);
                }
                else
                {
                    fillError(_error,
                              ErrorCode::FairMQChangeStateFailed,
                              string("FairMQ change state failed: ") + _ec.message());
                    OLOG(ESeverity::error) << stateSummaryString(_state, _expectedState, info->m_topo);
                }
                cv.notify_all();
            });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

        if (waitStatus == std::cv_status::timeout)
        {
            success = false;
            string msg{ toString("Timed out waiting for change state ", _transition) };
            fillError(_error, ErrorCode::RequestTimeout, msg);
            OLOG(ESeverity::error) << msg << endl
                                   << stateSummaryString(
                                          info->m_fairmqTopology->GetCurrentState(), _expectedState, info->m_topo);
        }
        else
        {
            OLOG(ESeverity::info) << "Changed state to " << _aggregatedState << " via " << _transition
                                  << " transition for partition " << std::quoted(_partitionID);
        }
    }
    catch (exception& _e)
    {
        success = false;
        fillError(_error, ErrorCode::FairMQChangeStateFailed, string("Change state failed: ") + _e.what());
        OLOG(ESeverity::error) << stateSummaryString(
            info->m_fairmqTopology->GetCurrentState(), _expectedState, info->m_topo);
    }

    return success;
}

bool CControlService::SImpl::changeStateConfigure(const partitionID_t& _partitionID,
                                                  SError& _error,
                                                  const string& _path,
                                                  AggregatedTopologyState& _aggregatedState,
                                                  TopologyState* _topologyState)
{
    return changeState(_partitionID,
                       _error,
                       TopologyTransition::InitDevice,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       TopologyTransition::CompleteInit,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       TopologyTransition::Bind,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       TopologyTransition::Connect,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       TopologyTransition::InitTask,
                       _path,
                       _aggregatedState,
                       _topologyState);
}

bool CControlService::SImpl::changeStateReset(const partitionID_t& _partitionID,
                                              SError& _error,
                                              const string& _path,
                                              AggregatedTopologyState& _aggregatedState,
                                              TopologyState* _topologyState)
{
    return changeState(_partitionID,
                       _error,
                       TopologyTransition::ResetTask,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       TopologyTransition::ResetDevice,
                       _path,
                       _aggregatedState,
                       _topologyState);
}

bool CControlService::SImpl::getState(const partitionID_t& _partitionID,
                                      SError& _error,
                                      const string& _path,
                                      AggregatedTopologyState& _aggregatedState,
                                      TopologyState* _topologyState)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    if (info->m_fairmqTopology == nullptr)
    {
        fillError(_error, ErrorCode::FairMQGetStateFailed, "FairMQ topology is not initialized");
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
        fillError(_error, ErrorCode::FairMQGetStateFailed, string("Get state failed: ") + _e.what());
    }
    if (_topologyState != nullptr)
        fairMQToODCTopologyState(info->m_topo, state, _topologyState);

    return success;
}

bool CControlService::SImpl::setProperties(const partitionID_t& _partitionID,
                                           SError& _error,
                                           const SSetPropertiesParams& _params)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    if (info->m_fairmqTopology == nullptr)
    {
        fillError(_error, ErrorCode::FairMQSetPropertiesFailed, "FairMQ topology is not initialized");
        return false;
    }

    bool success(true);

    try
    {
        std::condition_variable cv;

        info->m_fairmqTopology->AsyncSetProperties(
            _params.m_properties,
            _params.m_path,
            m_timeout,
            [&cv, &success, &_error, this](std::error_code _ec, FailedDevices)
            {
                success = !_ec;
                if (success)
                {
                    OLOG(ESeverity::info) << "Set property finished successfully";
                }
                else
                {
                    fillError(_error,
                              ErrorCode::FairMQSetPropertiesFailed,
                              string("Set property error message: ") + _ec.message());
                }
                cv.notify_all();
            });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

        if (waitStatus == std::cv_status::timeout)
        {
            success = false;
            fillError(_error, ErrorCode::RequestTimeout, "Timed out waiting for set property");
        }
        else
        {
            OLOG(ESeverity::info) << "Set property done successfully";
        }
    }
    catch (exception& _e)
    {
        success = false;
        fillError(_error, ErrorCode::FairMQSetPropertiesFailed, string("Set property failed: ") + _e.what());
    }

    return success;
}

AggregatedTopologyState CControlService::SImpl::aggregateStateForPath(
    const DDSTopologyPtr_t& _topo, const FairMQTopologyState& _topoState, const string& _path)
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
                         [&](const FairMQTopologyState::value_type& _v)
                         { return _v.taskId == task.m_taskId; }) };
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
        AggregatedTopologyState first{ static_cast<AggregatedTopologyState>(
            firstIt->state) };
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

void CControlService::SImpl::fillError(SError& _error, ErrorCode _errorCode, const string& _msg)
{
    _error.m_code = MakeErrorCode(_errorCode);
    _error.m_details = _msg;
    OLOG(ESeverity::error) << _error;
}

CControlService::SImpl::SSessionInfo::Ptr_t CControlService::SImpl::getOrCreateSessionInfo(
    const partitionID_t& _partitionID)
{
    auto it{ m_sessions.find(_partitionID) };
    if (it == m_sessions.end())
    {
        auto newSessionInfo{ make_shared<SSessionInfo>() };
        newSessionInfo->m_session = make_shared<CSession>();
        newSessionInfo->m_partitionID = _partitionID;
        m_sessions.insert(pair<partitionID_t, SSessionInfo::Ptr_t>(_partitionID, newSessionInfo));
        return newSessionInfo;
    }
    return it->second;
}

SError CControlService::SImpl::checkSessionIsRunning(const partitionID_t& _partitionID, ErrorCode _errorCode)
{
    SError error;
    auto info{ getOrCreateSessionInfo(_partitionID) };
    if (!info->m_session->IsRunning())
    {
        fillError(error, _errorCode, "DDS session is not running. Use Init or Run to start the session.");
    }
    return error;
}

string CControlService::SImpl::stateSummaryString(const FairMQTopologyState& _topologyState,
                                                  DeviceState _expectedState,
                                                  DDSTopologyPtr_t _topo)
{
    size_t totalCount{ _topologyState.size() };
    size_t failedCount{ 0 };
    stringstream ss;
    for (const auto& status : _topologyState)
    {
        // Print only failed devices
        if (status.state == _expectedState)
            continue;

        failedCount++;
        if (failedCount == 1)
        {
            ss << "List of failed devices for an expected state " << _expectedState << ":";
        }
        ss << endl
           << "  "
           << "Device: state (" << status.state << "), last state (" << status.lastState << "), task ID ("
           << status.taskId << "), collection ID (" << status.collectionId << "), "
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
            OLOG(ESeverity::error) << "Failed to get task with ID (" << status.taskId << ") from topology ("
                                   << _topo->getName() << ") at filepath " << std::quoted(_topo->getFilepath())
                                   << ". Error: " << _e.what();
        }
    }
    size_t successCount{ totalCount - failedCount };
    ss << endl
       << "Device status summary for expected state (" << _expectedState
       << "): total/success/failed devices (" << totalCount << "/" << successCount << "/" << failedCount << ")";

    return ss.str();
}

bool CControlService::SImpl::subscribeToDDSSession(const partitionID_t& _partitionID, SError& _error)
{
    try
    {
        auto info{ getOrCreateSessionInfo(_partitionID) };
        if (info->m_session->IsRunning())
        {
            // Subscrube on TaskDone events
            auto request{ SOnTaskDoneRequest::makeRequest(SOnTaskDoneRequest::request_t()) };
            request->setResponseCallback(
                [_partitionID](const SOnTaskDoneResponseData& _info)
                {
                    ESeverity severity{ (_info.m_exitCode != 0 || _info.m_signal != 0) ? ESeverity::error
                                                                                       : ESeverity::debug };
                    OLOG(severity) << "Partition " << quoted(_partitionID) << ": task (" << _info.m_taskID
                                   << ") exited with code (" << _info.m_exitCode << ") and signal (" << _info.m_signal
                                   << ")";
                });
            info->m_session->sendRequest<SOnTaskDoneRequest>(request);
            OLOG(ESeverity::info) << "Partition " << std::quoted(_partitionID)
                                  << " subscribed to task done event from session "
                                  << quoted(to_string(info->m_session->getSessionID()));
        }
        else
        {
            fillError(_error,
                      ErrorCode::DDSSubscribeToSessionFailed,
                      "Failed to subscribe to task done events: session is not running");
            return false;
        }
    }
    catch (exception& _e)
    {
        fillError(_error,
                  ErrorCode::DDSSubscribeToSessionFailed,
                  string("Failed to subscribe to task done events: ") + _e.what());
        return false;
    }
    return true;
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
        return _os << "ActivateParams: topologyFile=" << quoted(_params.m_topologyFile);
    }

    std::ostream& operator<<(std::ostream& _os, const SUpdateParams& _params)
    {
        return _os << "UpdateParams: topologyFile=" << quoted(_params.m_topologyFile);
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

    std::ostream& operator<<(std::ostream& _os, const SStatusParams& /* _params */)
    {
        return _os << "StatusParams";
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

SReturnValue CControlService::execInitialize(const partitionID_t& _partitionID, const SInitializeParams& _params)
{
    return m_impl->execInitialize(_partitionID, _params);
}

SReturnValue CControlService::execSubmit(const partitionID_t& _partitionID, const SSubmitParams& _params)
{
    return m_impl->execSubmit(_partitionID, _params);
}

SReturnValue CControlService::execActivate(const partitionID_t& _partitionID, const SActivateParams& _params)
{
    return m_impl->execActivate(_partitionID, _params);
}

SReturnValue CControlService::execRun(const partitionID_t& _partitionID,
                                      const SInitializeParams& _initializeParams,
                                      const SSubmitParams& _submitParams,
                                      const SActivateParams& _activateParams)
{
    return m_impl->execRun(_partitionID, _initializeParams, _submitParams, _activateParams);
}

SReturnValue CControlService::execUpdate(const partitionID_t& _partitionID, const SUpdateParams& _params)
{
    return m_impl->execUpdate(_partitionID, _params);
}

SReturnValue CControlService::execShutdown(const partitionID_t& _partitionID)
{
    return m_impl->execShutdown(_partitionID);
}

SReturnValue CControlService::execSetProperties(const partitionID_t& _partitionID, const SSetPropertiesParams& _params)
{
    return m_impl->execSetProperties(_partitionID, _params);
}

SReturnValue CControlService::execGetState(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return m_impl->execGetState(_partitionID, _params);
}

SReturnValue CControlService::execConfigure(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return m_impl->execConfigure(_partitionID, _params);
}

SReturnValue CControlService::execStart(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return m_impl->execStart(_partitionID, _params);
}

SReturnValue CControlService::execStop(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return m_impl->execStop(_partitionID, _params);
}

SReturnValue CControlService::execReset(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return m_impl->execReset(_partitionID, _params);
}

SReturnValue CControlService::execTerminate(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return m_impl->execTerminate(_partitionID, _params);
}

SStatusReturnValue CControlService::execStatus(const SStatusParams& _params)
{
    return m_impl->execStatus(_params);
}
