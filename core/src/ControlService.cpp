// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "ControlService.h"
#include "Error.h"
#include "Logger.h"
#include "TimeMeasure.h"
// FairMQ
#include <fairmq/SDK.h>
#include <fairmq/sdk/Topology.h>
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
    using FairMQTopologyPtr_t = std::shared_ptr<fair::mq::sdk::Topology>;

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

  private:
    SReturnValue createReturnValue(const partitionID_t& _partitionID,
                                   const SError& _error,
                                   const std::string& _msg,
                                   size_t _execTime,
                                   fair::mq::sdk::AggregatedTopologyState _aggregatedState,
                                   SReturnDetails::ptr_t _details = nullptr);
    bool createDDSSession(const partitionID_t& _partitionID, SError& _error);
    bool attachToDDSSession(const partitionID_t& _partitionID, SError& _error, const std::string& _sessionID);
    bool submitDDSAgents(const partitionID_t& _partitionID, SError& _error, const SSubmitParams& _params);
    bool activateDDSTopology(const partitionID_t& _partitionID,
                             SError& _error,
                             const std::string& _topologyFile,
                             dds::tools_api::STopologyRequest::request_t::EUpdateType _updateType);
    bool waitForNumActiveAgents(const partitionID_t& _partitionID, SError& _error, size_t _numAgents);
    bool requestCommanderInfo(const partitionID_t& _partitionID,
                              SError& _error,
                              SCommanderInfoRequest::response_t& _commanderInfo);
    bool shutdownDDSSession(const partitionID_t& _partitionID, SError& _error);
    bool createFairMQTopo(const partitionID_t& _partitionID, SError& _error, const std::string& _topologyFile);
    bool createTopo(const partitionID_t& _partitionID, SError& _error, const std::string& _topologyFile);
    bool setProperties(const partitionID_t& _partitionID, SError& _error, const SSetPropertiesParams& _params);
    bool changeState(const partitionID_t& _partitionID,
                     SError& _error,
                     fair::mq::sdk::TopologyTransition _transition,
                     const std::string& _path,
                     fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
                     TopologyState* _topologyState = nullptr);
    bool getState(const partitionID_t& _partitionID,
                  SError& _error,
                  const string& _path,
                  fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
                  TopologyState* _topologyState = nullptr);
    bool changeStateConfigure(const partitionID_t& _partitionID,
                              SError& _error,
                              const std::string& _path,
                              fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
                              TopologyState* _topologyState = nullptr);
    bool changeStateReset(const partitionID_t& _partitionID,
                          SError& _error,
                          const std::string& _path,
                          fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
                          TopologyState* _topologyState = nullptr);

    void resetTopology(const partitionID_t& _partitionID);

    void fillError(SError& _error, ErrorCode _errorCode, const string& _msg);

    fair::mq::sdk::AggregatedTopologyState aggregateStateForPath(const DDSTopologyPtr_t& _topo,
                                                                 const fair::mq::sdk::TopologyState& _fairmq,
                                                                 const string& _path);
    void fairMQToODCTopologyState(const DDSTopologyPtr_t& _topo,
                                  const fair::mq::sdk::TopologyState& _fairmq,
                                  TopologyState* _odc);

    SSessionInfo::Ptr_t getOrCreateSessionInfo(const partitionID_t& _partitionID);

    // Disable copy constructors and assignment operators
    SImpl(const SImpl&) = delete;
    SImpl(SImpl&&) = delete;
    SImpl& operator=(const SImpl&) = delete;
    SImpl& operator=(SImpl&&) = delete;

    fair::mq::sdk::DDSEnv
        m_fairmqEnv; ///< FairMQ environment. We store it globally because only one instance per process is allowed.
    SSessionInfo::Map_t m_sessions;  ///< Map of partition ID to session info
    chrono::seconds m_timeout{ 30 }; ///< Request timeout in sec
};

SReturnValue CControlService::SImpl::execInitialize(const partitionID_t& _partitionID, const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error;
    if (_params.m_sessionID.empty())
    {
        // Shutdown DDS session if it is running already
        // Create new DDS session
        shutdownDDSSession(_partitionID, error) && createDDSSession(_partitionID, error);
    }
    else
    {
        // Shutdown DDS session if it is running already
        // Attach to an existing DDS session
        bool success{ shutdownDDSSession(_partitionID, error) &&
                      attachToDDSSession(_partitionID, error, _params.m_sessionID) };

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
        _partitionID, error, "Initialize done", measure.duration(), fair::mq::sdk::AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execSubmit(const partitionID_t& _partitionID, const SSubmitParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    size_t allCount = _params.m_numAgents * _params.m_numSlots;
    // Submit DDS agents
    // Wait until all agents are active
    SError error;
    submitDDSAgents(_partitionID, error, _params) && waitForNumActiveAgents(_partitionID, error, allCount);
    return createReturnValue(
        _partitionID, error, "Submit done", measure.duration(), fair::mq::sdk::AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execActivate(const partitionID_t& _partitionID, const SActivateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Activate DDS topology
    // Create fair::mq::sdk::Topology
    SError error;
    activateDDSTopology(
        _partitionID, error, _params.m_topologyFile, STopologyRequest::request_t::EUpdateType::ACTIVATE) &&
        createTopo(_partitionID, error, _params.m_topologyFile) &&
        createFairMQTopo(_partitionID, error, _params.m_topologyFile);
    fair::mq::sdk::AggregatedTopologyState state{ !error.m_code ? fair::mq::sdk::AggregatedTopologyState::Idle
                                                                : fair::mq::sdk::AggregatedTopologyState::Undefined };
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
        SError error{ execInitialize(_partitionID, _initializeParams).m_error };
        if (!error.m_code)
        {
            error = execSubmit(_partitionID, _submitParams).m_error;
            if (!error.m_code)
            {
                error = execActivate(_partitionID, _activateParams).m_error;
            }
        }
    }
    fair::mq::sdk::AggregatedTopologyState state{ !error.m_code ? fair::mq::sdk::AggregatedTopologyState::Idle
                                                                : fair::mq::sdk::AggregatedTopologyState::Undefined };
    return createReturnValue(_partitionID, error, "Run done", measure.duration(), state);
}

SReturnValue CControlService::SImpl::execUpdate(const partitionID_t& _partitionID, const SUpdateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    fair::mq::sdk::AggregatedTopologyState state{ fair::mq::sdk::AggregatedTopologyState::Undefined };
    // Reset devices' state
    // Update DDS topology
    // Create fair::mq::sdk::Topology
    // Configure devices' state
    SError error;
    changeStateReset(_partitionID, error, "", state) &&
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
        _partitionID, error, "Shutdown done", measure.duration(), fair::mq::sdk::AggregatedTopologyState::Undefined);
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
                             fair::mq::sdk::AggregatedTopologyState::Undefined);
}

SReturnValue CControlService::SImpl::execGetState(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    fair::mq::sdk::AggregatedTopologyState state{ fair::mq::sdk::AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    getState(_partitionID, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "GetState done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execConfigure(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    fair::mq::sdk::AggregatedTopologyState state{ fair::mq::sdk::AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeStateConfigure(
        _partitionID, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "ConfigureRun done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execStart(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    fair::mq::sdk::AggregatedTopologyState state{ fair::mq::sdk::AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_partitionID,
                error,
                fair::mq::sdk::TopologyTransition::Run,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Start done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execStop(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    fair::mq::sdk::AggregatedTopologyState state{ fair::mq::sdk::AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_partitionID,
                error,
                fair::mq::sdk::TopologyTransition::Stop,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Stop done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execReset(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    fair::mq::sdk::AggregatedTopologyState state{ fair::mq::sdk::AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeStateReset(
        _partitionID, error, _params.m_path, state, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Reset done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::execTerminate(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    fair::mq::sdk::AggregatedTopologyState state{ fair::mq::sdk::AggregatedTopologyState::Undefined };
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    SError error;
    changeState(_partitionID,
                error,
                fair::mq::sdk::TopologyTransition::End,
                _params.m_path,
                state,
                ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(_partitionID, error, "Terminate done", measure.duration(), state, details);
}

SReturnValue CControlService::SImpl::createReturnValue(const partitionID_t& _partitionID,
                                                       const SError& _error,
                                                       const std::string& _msg,
                                                       size_t _execTime,
                                                       fair::mq::sdk::AggregatedTopologyState _aggregatedState,
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
                                             const SSubmitParams& _params)
{
    bool success(true);

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_rms = _params.m_rmsPlugin;
    requestInfo.m_instances = _params.m_numAgents;
    requestInfo.m_slots = _params.m_numSlots;
    if (_params.m_rmsPlugin != "localhost")
    {
        requestInfo.m_config = _params.m_configFile;
    }

    std::condition_variable cv;

    SSubmitRequest::ptr_t requestPtr = SSubmitRequest::makeRequest(requestInfo);

    requestPtr->setMessageCallback([&success, &_error, this](const SMessageResponseData& _message) {
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

    requestPtr->setDoneCallback([&cv]() {
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

    STopologyRequest::ptr_t requestPtr = STopologyRequest::makeRequest(topoInfo);

    requestPtr->setMessageCallback([&success, &_error, this](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
        {
            success = false;
            fillError(_error, ErrorCode::DDSActivateTopologyFailed, string("Server reports error: ") + _message.m_msg);
        }
        else
        {
            OLOG(ESeverity::debug) << "Server reports: " << _message.m_msg;
        }
    });

    requestPtr->setProgressCallback([](const SProgressResponseData& _progress) {
        int completed = _progress.m_completed + _progress.m_errors;
        if (completed == _progress.m_total)
        {
            OLOG(ESeverity::info) << "Activated tasks: " << _progress.m_completed << "\nErrors: " << _progress.m_errors
                                  << "\nTotal: " << _progress.m_total;
        }
    });

    requestPtr->setDoneCallback([&cv]() {
        OLOG(ESeverity::info) << "Topology activation done";
        cv.notify_all();
    });

    auto info{ getOrCreateSessionInfo(_partitionID) };
    info->m_session->sendRequest<STopologyRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        fillError(_error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
        OLOG(ESeverity::error) << _error;
    }
    else
    {
        OLOG(ESeverity::info) << "Topology activation done successfully";
    }
    return success;
}

bool CControlService::SImpl::shutdownDDSSession(const partitionID_t& _partitionID, SError& _error)
{
    try
    {
        // Reset topology on shutdown
        resetTopology(_partitionID);

        auto info{ getOrCreateSessionInfo(_partitionID) };
        if (info->m_session->IsRunning())
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
    }
    catch (exception& _e)
    {
        fillError(
            _error, ErrorCode::DDSCreateTopologyFailed, string("Failed to initialize DDS topology: ") + _e.what());
        return false;
    }
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
        fair::mq::sdk::DDSSession session(info->m_session, m_fairmqEnv);
        session.StopOnDestruction(false);
        fair::mq::sdk::DDSTopo topo(fair::mq::sdk::DDSTopo::Path(_topologyFile), m_fairmqEnv);
        info->m_fairmqTopology = make_shared<fair::mq::sdk::Topology>(topo, session);
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

void CControlService::SImpl::resetTopology(const partitionID_t& _partitionID)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    info->m_topo.reset();
    info->m_fairmqTopology.reset();
}

bool CControlService::SImpl::changeState(const partitionID_t& _partitionID,
                                         SError& _error,
                                         fair::mq::sdk::TopologyTransition _transition,
                                         const string& _path,
                                         fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
                                         TopologyState* _topologyState)
{
    auto info{ getOrCreateSessionInfo(_partitionID) };
    if (info->m_fairmqTopology == nullptr)
    {
        fillError(_error, ErrorCode::FairMQChangeStateFailed, "FairMQ topology is not initialized");
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
            [&cv, &success, &_aggregatedState, &_topologyState, &_error, &info, this](
                std::error_code _ec, fair::mq::sdk::TopologyState _state) {
                success = !_ec;
                if (success)
                {
                    try
                    {
                        _aggregatedState = fair::mq::sdk::AggregateState(_state);
                        OLOG(ESeverity::info) << "Aggregated topology state";
                    }
                    catch (exception& _e)
                    {
                        success = false;
                        fillError(_error,
                                  ErrorCode::FairMQChangeStateFailed,
                                  string("Aggregate topology state failed: ") + _e.what());
                    }
                    if (_topologyState != nullptr)
                        fairMQToODCTopologyState(info->m_topo, _state, _topologyState);
                }
                else
                {
                    fillError(_error,
                              ErrorCode::FairMQChangeStateFailed,
                              string("FairMQ change state failed: ") + _ec.message());
                }
                cv.notify_all();
            });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

        if (waitStatus == std::cv_status::timeout)
        {
            success = false;
            fillError(_error,
                      ErrorCode::RequestTimeout,
                      "Timed out waiting for change state " + fair::mq::GetTransitionName(_transition));
        }
        else
        {
            OLOG(ESeverity::info) << "Change state done successfully " << _transition;
        }
    }
    catch (exception& _e)
    {
        success = false;
        fillError(_error, ErrorCode::FairMQChangeStateFailed, string("Change state failed: ") + _e.what());
    }

    return success;
}

bool CControlService::SImpl::changeStateConfigure(const partitionID_t& _partitionID,
                                                  SError& _error,
                                                  const string& _path,
                                                  fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
                                                  TopologyState* _topologyState)
{
    return changeState(_partitionID,
                       _error,
                       fair::mq::sdk::TopologyTransition::InitDevice,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       fair::mq::sdk::TopologyTransition::CompleteInit,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       fair::mq::sdk::TopologyTransition::Bind,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       fair::mq::sdk::TopologyTransition::Connect,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       fair::mq::sdk::TopologyTransition::InitTask,
                       _path,
                       _aggregatedState,
                       _topologyState);
}

bool CControlService::SImpl::changeStateReset(const partitionID_t& _partitionID,
                                              SError& _error,
                                              const string& _path,
                                              fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
                                              TopologyState* _topologyState)
{
    return changeState(_partitionID,
                       _error,
                       fair::mq::sdk::TopologyTransition::ResetTask,
                       _path,
                       _aggregatedState,
                       _topologyState) &&
           changeState(_partitionID,
                       _error,
                       fair::mq::sdk::TopologyTransition::ResetDevice,
                       _path,
                       _aggregatedState,
                       _topologyState);
}

bool CControlService::SImpl::getState(const partitionID_t& _partitionID,
                                      SError& _error,
                                      const string& _path,
                                      fair::mq::sdk::AggregatedTopologyState& _aggregatedState,
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
            [&cv, &success, &_error, this](std::error_code _ec, fair::mq::sdk::FailedDevices) {
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

fair::mq::sdk::AggregatedTopologyState CControlService::SImpl::aggregateStateForPath(
    const DDSTopologyPtr_t& _topo, const fair::mq::sdk::TopologyState& _topoState, const string& _path)
{
    if (_path.empty())
        return fair::mq::sdk::AggregateState(_topoState);

    if (_topo == nullptr)
        throw runtime_error("DDS topology is not initialized");

    try
    {
        // Check if path points to a single task
        // If task is found than return it's state as an aggregated topology state

        // Throws if task not found for path
        const auto& task{ _topo->getRuntimeTask(_path) };
        auto it{ find_if(
            _topoState.cbegin(), _topoState.cend(), [&](const fair::mq::sdk::TopologyState::value_type& _v) {
                return _v.taskId == task.m_taskId;
            }) };
        if (it != _topoState.cend())
            return static_cast<fair::mq::sdk::AggregatedTopologyState>(it->state);

        throw runtime_error("Device not found for path " + _path);
    }
    catch (exception& _e)
    {
        // In case of exception check that path contains multiple tasks

        // Collect all task IDs to a set for fast search
        set<Id_t> taskIds;
        auto it{ _topo->getRuntimeTaskIteratorMatchingPath(_path) };
        for_each(it.first, it.second, [&](const STopoRuntimeTask::FilterIterator_t::value_type& _v) {
            taskIds.insert(_v.second.m_taskId);
        });

        if (taskIds.empty())
            throw runtime_error("No tasks found matching the path " + _path);

        // Find a state of a first task
        auto firstIt{ find_if(
            _topoState.cbegin(), _topoState.cend(), [&](const fair::mq::sdk::TopologyState::value_type& _v) {
                return _v.taskId == *(taskIds.begin());
            }) };
        if (firstIt == _topoState.cend())
            throw runtime_error("No states found for path " + _path);

        // Check that all selected devices have the same state
        fair::mq::sdk::AggregatedTopologyState first{ static_cast<fair::mq::sdk::AggregatedTopologyState>(
            firstIt->state) };
        if (std::all_of(
                _topoState.cbegin(), _topoState.cend(), [&](const fair::mq::sdk::TopologyState::value_type& _v) {
                    return (taskIds.count(_v.taskId) > 0) ? _v.state == first : true;
                }))
        {
            return first;
        }

        return fair::mq::sdk::AggregatedTopologyState::Mixed;
    }
}

void CControlService::SImpl::fairMQToODCTopologyState(const DDSTopologyPtr_t& _topo,
                                                      const fair::mq::sdk::TopologyState& _fairmq,
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

//
// Misc
//

namespace odc
{
    namespace core
    {
        std::ostream& operator<<(std::ostream& _os, const SError& _error)
        {
            return _os << _error.m_code << " (" << _error.m_details << ")";
        }
    } // namespace core
} // namespace odc
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
