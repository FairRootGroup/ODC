// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "ControlService.h"
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
    // TODO: FIXME: Implement sanity check before calling API
    SReturnValue execInitialize(const SInitializeParams& _params);
    SReturnValue execSubmit(const SSubmitParams& _params);
    SReturnValue execActivate(const SActivateParams& _params);
    SReturnValue execUpdate(const SUpdateParams& _params);
    SReturnValue execShutdown();

    SReturnValue execSetProperty(const SSetPropertyParams& _params);

    SReturnValue execConfigure(const SDeviceParams& _params);
    SReturnValue execStart(const SDeviceParams& _params);
    SReturnValue execStop(const SDeviceParams& _params);
    SReturnValue execReset(const SDeviceParams& _params);
    SReturnValue execTerminate(const SDeviceParams& _params);

  private:
    SReturnValue createReturnValue(bool _success,
                                   const std::string& _msg,
                                   const std::string& _errMsg,
                                   size_t _execTime,
                                   SReturnDetails::ptr_t _details = nullptr);
    bool createDDSSession();
    bool attachToDDSSession(const std::string& _sessionID);
    bool submitDDSAgents(const SSubmitParams& _params);
    bool activateDDSTopology(const std::string& _topologyFile,
                             dds::tools_api::STopologyRequest::request_t::EUpdateType _updateType);
    bool waitForNumActiveAgents(size_t _numAgents);
    bool requestCommanderInfo(SCommanderInfoRequest::response_t& _commanderInfo);
    bool shutdownDDSSession();
    bool createFairMQTopo(const std::string& _topologyFile);
    bool createTopo(const std::string& _topologyFile);
    bool setProperty(const SSetPropertyParams& _params);
    bool changeState(fair::mq::sdk::TopologyTransition _transition,
                     const std::string& _path,
                     TopologyState* _topologyState = nullptr);
    bool changeStateConfigure(const std::string& _path, TopologyState* _topologyState = nullptr);
    bool changeStateReset(const std::string& _path, TopologyState* _topologyState = nullptr);

    void fairMQToODCTopologyState(const fair::mq::sdk::TopologyState& _fairmq, TopologyState* _odc);

    // Disable copy constructors and assignment operators
    SImpl(const SImpl&) = delete;
    SImpl(SImpl&&) = delete;
    SImpl& operator=(const SImpl&) = delete;
    SImpl& operator=(SImpl&&) = delete;

    DDSTopologyPtr_t m_topo{ nullptr };                   ///< DDS topology
    DDSSessionPtr_t m_session{ make_shared<CSession>() }; ///< DDS session
    FairMQTopologyPtr_t m_fairmqTopology{ nullptr };      ///< FairMQ topology
    chrono::seconds m_timeout{ 30 };                      ///< Request timeout in sec
    runID_t m_runID{ 0 };                                 ///< Current external runID for this session
};

SReturnValue CControlService::SImpl::execInitialize(const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Set current run ID
    m_runID = _params.m_runID;

    bool success{ true };
    if (_params.m_sessionID.empty())
    {
        // Shutdown DDS session if it is running already
        // Create new DDS session
        success = shutdownDDSSession() && createDDSSession();
    }
    else
    {
        // Shutdown DDS session if it is running already
        // Attach to an existing DDS session
        success = shutdownDDSSession() && attachToDDSSession(_params.m_sessionID);

        // Request current active topology, if any
        if (success)
        {
            SCommanderInfoRequest::response_t commanderInfo;
            success = requestCommanderInfo(commanderInfo);

            // If topology is active, create FairMQ topology
            if (success && !commanderInfo.m_activeTopologyPath.empty())
            {
                success = createFairMQTopo(commanderInfo.m_activeTopologyPath);
            }
        }
    }
    return createReturnValue(success, "Initialize done", "Initialize failed", measure.duration());
}

SReturnValue CControlService::SImpl::execSubmit(const SSubmitParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    size_t allCount = _params.m_numAgents * _params.m_numSlots;
    // Submit DDS agents
    // Wait until all agents are active
    bool success = submitDDSAgents(_params) && waitForNumActiveAgents(allCount);
    return createReturnValue(success, "Submit done", "Submit failed", measure.duration());
}

SReturnValue CControlService::SImpl::execActivate(const SActivateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Activate DDS topology
    // Create fair::mq::sdk::Topology
    bool success = activateDDSTopology(_params.m_topologyFile, STopologyRequest::request_t::EUpdateType::ACTIVATE) &&
                   createTopo(_params.m_topologyFile) && createFairMQTopo(_params.m_topologyFile);
    return createReturnValue(success, "Activate done", "Activate failed", measure.duration());
}

SReturnValue CControlService::SImpl::execUpdate(const SUpdateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Reset devices' state
    // Update DDS topology
    // Create fair::mq::sdk::Topology
    // Configure devices' state
    bool success = changeStateReset("") &&
                   activateDDSTopology(_params.m_topologyFile, STopologyRequest::request_t::EUpdateType::UPDATE) &&
                   createTopo(_params.m_topologyFile) && createFairMQTopo(_params.m_topologyFile) &&
                   changeStateConfigure("");
    return createReturnValue(success, "Update done", "Update failed", measure.duration());
}

SReturnValue CControlService::SImpl::execShutdown()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = shutdownDDSSession();
    return createReturnValue(success, "Shutdown done", "Shutdown failed", measure.duration());
}

SReturnValue CControlService::SImpl::execSetProperty(const SSetPropertyParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = setProperty(_params);
    return createReturnValue(success, "ConfigureRun done", "ConfigureRun failed", measure.duration());
}

SReturnValue CControlService::SImpl::execConfigure(const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    bool success = changeStateConfigure(_params.m_path, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(success, "ConfigureRun done", "ConfigureRun failed", measure.duration(), details);
}

SReturnValue CControlService::SImpl::execStart(const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    bool success = changeState(fair::mq::sdk::TopologyTransition::Run,
                               _params.m_path,
                               ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(success, "Start done", "Start failed", measure.duration(), details);
}

SReturnValue CControlService::SImpl::execStop(const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    bool success = changeState(fair::mq::sdk::TopologyTransition::Stop,
                               _params.m_path,
                               ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(success, "Stop done", "Stop failed", measure.duration(), details);
}

SReturnValue CControlService::SImpl::execReset(const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    bool success = changeStateReset(_params.m_path, ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(success, "Reset done", "Reset failed", measure.duration(), details);
}

SReturnValue CControlService::SImpl::execTerminate(const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SReturnDetails::ptr_t details((_params.m_detailed) ? make_shared<SReturnDetails>() : nullptr);
    bool success = changeState(fair::mq::sdk::TopologyTransition::End,
                               _params.m_path,
                               ((details == nullptr) ? nullptr : &details->m_topologyState));
    return createReturnValue(success, "Terminate done", "Terminate failed", measure.duration(), details);
}

SReturnValue CControlService::SImpl::createReturnValue(bool _success,
                                                       const std::string& _msg,
                                                       const std::string& _errMsg,
                                                       size_t _execTime,
                                                       SReturnDetails::ptr_t _details)
{
    string sidStr{ to_string(m_session->getSessionID()) };
    if (_success)
    {
        return SReturnValue(EStatusCode::ok, _msg, _execTime, SError(), m_runID, sidStr, _details);
    }
    return SReturnValue(EStatusCode::error, "", _execTime, SError(123, _errMsg), m_runID, sidStr, _details);
}

bool CControlService::SImpl::createDDSSession()
{
    bool success(true);
    try
    {
        boost::uuids::uuid sessionID = m_session->create();
        OLOG(ESeverity::info) << "DDS session created with session ID: " << to_string(sessionID);
    }
    catch (exception& _e)
    {
        success = false;
        OLOG(ESeverity::error) << "Failed to create DDS session: " << _e.what();
    }
    return success;
}

bool CControlService::SImpl::attachToDDSSession(const std::string& _sessionID)
{
    bool success(true);
    try
    {
        m_session->attach(_sessionID);
        OLOG(ESeverity::info) << "Attach to a DDS session with session ID: " << _sessionID;
    }
    catch (exception& _e)
    {
        success = false;
        OLOG(ESeverity::error) << "Failed to attach to a DDS session: " << _e.what();
    }
    return success;
}

bool CControlService::SImpl::submitDDSAgents(const SSubmitParams& _params)
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

    requestPtr->setMessageCallback([&success](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
        {
            success = false;
            OLOG(ESeverity::error) << "Server reports error: " << _message.m_msg;
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

    m_session->sendRequest<SSubmitRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        OLOG(ESeverity::error) << "Timed out waiting for agent submission";
    }
    else
    {
        OLOG(ESeverity::info) << "Agent submission done successfully";
    }
    return success;
}

bool CControlService::SImpl::requestCommanderInfo(SCommanderInfoRequest::response_t& _commanderInfo)
{
    try
    {
        stringstream ss;
        m_session->syncSendRequest<SCommanderInfoRequest>(
            SCommanderInfoRequest::request_t(), _commanderInfo, m_timeout, &ss);
        OLOG(ESeverity::info) << ss.str();
        OLOG(ESeverity::debug) << "Commander info: " << _commanderInfo;
        return true;
    }
    catch (exception& _e)
    {
        OLOG(ESeverity::error) << "Error getting DDS commander info: " << _e.what();
        return false;
    }
}

bool CControlService::SImpl::waitForNumActiveAgents(size_t _numAgents)
{
    try
    {
        m_session->waitForNumAgents<CSession::EAgentState::active>(_numAgents, m_timeout);
    }
    catch (std::exception& _e)
    {
        OLOG(ESeverity::error) << "Timeout waiting for DDS agents: " << _e.what();
        return false;
    }
    return true;
}

bool CControlService::SImpl::activateDDSTopology(const string& _topologyFile,
                                                 STopologyRequest::request_t::EUpdateType _updateType)
{
    bool success(true);

    STopologyRequest::request_t topoInfo;
    topoInfo.m_topologyFile = _topologyFile;
    topoInfo.m_disableValidation = true;
    topoInfo.m_updateType = _updateType;

    std::condition_variable cv;

    STopologyRequest::ptr_t requestPtr = STopologyRequest::makeRequest(topoInfo);

    requestPtr->setMessageCallback([&success](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
        {
            success = false;
            OLOG(ESeverity::error) << "Server reports error: " << _message.m_msg;
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

    m_session->sendRequest<STopologyRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        OLOG(ESeverity::error) << "Timed out waiting for agent submission";
    }
    else
    {
        OLOG(ESeverity::info) << "Topology activation done successfully";
    }
    return success;
}

bool CControlService::SImpl::shutdownDDSSession()
{
    bool success(true);
    try
    {
        if (m_session->IsRunning())
        {
            m_session->shutdown();
            if (m_session->getSessionID() == boost::uuids::nil_uuid())
            {
                OLOG(ESeverity::info) << "DDS session shutted down";
            }
            else
            {
                OLOG(ESeverity::error) << "Failed to shut down DDS session";
                success = false;
            }
        }
    }
    catch (exception& _e)
    {
        success = false;
        OLOG(ESeverity::error) << "Shutdown failed: " << _e.what();
    }
    return success;
}

bool CControlService::SImpl::createTopo(const std::string& _topologyFile)
{
    try
    {
        m_topo = make_shared<dds::topology_api::CTopology>(_topologyFile);
    }
    catch (exception& _e)
    {
        OLOG(ESeverity::error) << "Failed to initialize DDS topology: " << _e.what();
        return false;
    }
    return true;
}

bool CControlService::SImpl::createFairMQTopo(const std::string& _topologyFile)
{
    try
    {
        m_fairmqTopology.reset();
        fair::mq::sdk::DDSEnv env;
        fair::mq::sdk::DDSSession session(m_session, env);
        session.StopOnDestruction(false);
        fair::mq::sdk::DDSTopo topo(fair::mq::sdk::DDSTopo::Path(_topologyFile), env);
        m_fairmqTopology = make_shared<fair::mq::sdk::Topology>(topo, session);
    }
    catch (exception& _e)
    {
        m_fairmqTopology = nullptr;
        OLOG(ESeverity::error) << "Failed to initialize FairMQ topology: " << _e.what();
    }
    return m_fairmqTopology != nullptr;
}

bool CControlService::SImpl::changeState(fair::mq::sdk::TopologyTransition _transition,
                                         const string& _path,
                                         TopologyState* _topologyState)
{
    if (m_fairmqTopology == nullptr)
        return false;

    bool success(true);

    try
    {
        fair::mq::sdk::DeviceState targetState;
        std::condition_variable cv;

        m_fairmqTopology->AsyncChangeState(_transition,
                                           _path,
                                           m_timeout,
                                           [&cv, &success, &targetState, &_topologyState, this](
                                               std::error_code _ec, fair::mq::sdk::TopologyState _state) {
                                               OLOG(ESeverity::info) << "Change transition result: " << _ec.message();
                                               success = !_ec;
                                               try
                                               {
                                                   targetState = fair::mq::sdk::AggregateState(_state);
                                               }
                                               catch (exception& _e)
                                               {
                                                   success = false;
                                                   OLOG(ESeverity::error) << "Change state failed: " << _e.what();
                                               }
                                               if (_topologyState != nullptr)
                                                   fairMQToODCTopologyState(_state, _topologyState);
                                               cv.notify_all();
                                           });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

        if (waitStatus == std::cv_status::timeout)
        {
            success = false;
            OLOG(ESeverity::error) << "Timed out waiting for change state " << _transition;
        }
        else
        {
            OLOG(ESeverity::info) << "Change state done successfully " << _transition;
        }
    }
    catch (exception& _e)
    {
        success = false;
        OLOG(ESeverity::error) << "Change state failed: " << _e.what();
    }

    return success;
}

bool CControlService::SImpl::changeStateConfigure(const string& _path, TopologyState* _topologyState)
{
    return changeState(fair::mq::sdk::TopologyTransition::InitDevice, _path, _topologyState) &&
           changeState(fair::mq::sdk::TopologyTransition::CompleteInit, _path, _topologyState) &&
           changeState(fair::mq::sdk::TopologyTransition::Bind, _path, _topologyState) &&
           changeState(fair::mq::sdk::TopologyTransition::Connect, _path, _topologyState) &&
           changeState(fair::mq::sdk::TopologyTransition::InitTask, _path, _topologyState);
}

bool CControlService::SImpl::changeStateReset(const string& _path, TopologyState* _topologyState)
{
    return changeState(fair::mq::sdk::TopologyTransition::ResetTask, _path, _topologyState) &&
           changeState(fair::mq::sdk::TopologyTransition::ResetDevice, _path, _topologyState);
}

bool CControlService::SImpl::setProperty(const SSetPropertyParams& _params)
{
    if (m_fairmqTopology == nullptr)
        return false;

    bool success(true);

    try
    {
        std::condition_variable cv;

        m_fairmqTopology->AsyncSetProperties({ { _params.m_key, _params.m_value } },
                                             _params.m_path,
                                             m_timeout,
                                             [&cv, &success](std::error_code _ec, fair::mq::sdk::FailedDevices) {
                                                 OLOG(ESeverity::info) << "Set property result: " << _ec.message();
                                                 success = !_ec;
                                                 cv.notify_all();
                                             });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::cv_status waitStatus = cv.wait_for(lock, m_timeout);

        if (waitStatus == std::cv_status::timeout)
        {
            success = false;
            OLOG(ESeverity::error) << "Timed out waiting for set property";
        }
        else
        {
            OLOG(ESeverity::info) << "Set property done successfully";
        }
    }
    catch (exception& _e)
    {
        success = false;
        OLOG(ESeverity::error) << "Set property failed: " << _e.what();
    }

    return success;
}

void CControlService::SImpl::fairMQToODCTopologyState(const fair::mq::sdk::TopologyState& _fairmq, TopologyState* _odc)
{
    if (_odc == nullptr || m_topo == nullptr)
        return;

    _odc->reserve(_fairmq.size());
    for (const auto& state : _fairmq)
    {
        const auto& task = m_topo->getRuntimeTaskById(state.taskId);
        _odc->push_back(SDeviceStatus(state, task.m_taskPath));
    }
}

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

SReturnValue CControlService::execInitialize(const SInitializeParams& _params)
{
    return m_impl->execInitialize(_params);
}

SReturnValue CControlService::execSubmit(const SSubmitParams& _params)
{
    return m_impl->execSubmit(_params);
}

SReturnValue CControlService::execActivate(const SActivateParams& _params)
{
    return m_impl->execActivate(_params);
}

SReturnValue CControlService::execUpdate(const SUpdateParams& _params)
{
    return m_impl->execUpdate(_params);
}

SReturnValue CControlService::execShutdown()
{
    return m_impl->execShutdown();
}

SReturnValue CControlService::execSetProperty(const SSetPropertyParams& _params)
{
    return m_impl->execSetProperty(_params);
}

SReturnValue CControlService::execConfigure(const SDeviceParams& _params)
{
    return m_impl->execConfigure(_params);
}

SReturnValue CControlService::execStart(const SDeviceParams& _params)
{
    return m_impl->execStart(_params);
}

SReturnValue CControlService::execStop(const SDeviceParams& _params)
{
    return m_impl->execStop(_params);
}

SReturnValue CControlService::execReset(const SDeviceParams& _params)
{
    return m_impl->execReset(_params);
}

SReturnValue CControlService::execTerminate(const SDeviceParams& _params)
{
    return m_impl->execTerminate(_params);
}
