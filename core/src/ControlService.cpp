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
    using DDSSessionPtr_t = std::shared_ptr<dds::tools_api::CSession>;
    using FairMQTopologyPtr_t = std::shared_ptr<fair::mq::sdk::Topology>;

    SImpl()
        : m_session(make_shared<CSession>())
        , m_fairmqTopology(nullptr)
        , m_timeout(1800.)
    {
        //    fair::Logger::SetConsoleSeverity("debug");
    }

    ~SImpl()
    {
    }

    // Core API calls
    // TODO: FIXME: Implement sanity check before calling API
    SReturnValue execInitialize(const SInitializeParams& _params);
    SReturnValue execSubmit(const SSubmitParams& _params);
    SReturnValue execActivate(const SActivateParams& _params);
    SReturnValue execUpdate(const SUpdateParams& _params);
    SReturnValue execConfigure();
    SReturnValue execStart();
    SReturnValue execStop();
    SReturnValue execReset();
    SReturnValue execTerminate();
    SReturnValue execShutdown();

  private:
    SReturnValue createReturnValue(bool _success,
                                   const std::string& _msg,
                                   const std::string& _errMsg,
                                   size_t _execTime);
    bool createDDSSession();
    bool submitDDSAgents(const SSubmitParams& _params);
    bool activateDDSTopology(const std::string& _topologyFile,
                             dds::tools_api::STopologyRequest::request_t::EUpdateType _updateType);
    bool waitForNumActiveAgents(size_t _numAgents);
    bool shutdownDDSSession();
    bool createFairMQTopo(const std::string& _topologyFile);
    bool changeState(fair::mq::sdk::TopologyTransition _transition);
    bool changeStateConfigure();
    bool changeStateReset();

    // Disable copy constructors and assignment operators
    SImpl(const SImpl&) = delete;
    SImpl(SImpl&&) = delete;
    SImpl& operator=(const SImpl&) = delete;
    SImpl& operator=(SImpl&&) = delete;

    DDSSessionPtr_t m_session;            ///< DDS session
    FairMQTopologyPtr_t m_fairmqTopology; ///< FairMQ topology
    const size_t m_timeout;               ///< Request timeout in sec
    runID_t m_runID;                      ///< Current external runID for this session
};

SReturnValue CControlService::SImpl::execInitialize(const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Set current run ID
    m_runID = _params.m_runID;
    // Shutdown DDS session if it is running already
    // Create new DDS session
    bool success = shutdownDDSSession() && createDDSSession();
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
                   createFairMQTopo(_params.m_topologyFile);
    return createReturnValue(success, "Activate done", "Activate failed", measure.duration());
}

SReturnValue CControlService::SImpl::execUpdate(const SUpdateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Reset devices' state
    // Update DDS topology
    // Create fair::mq::sdk::Topology
    // Configure devices' state
    bool success = changeStateReset() &&
                   activateDDSTopology(_params.m_topologyFile, STopologyRequest::request_t::EUpdateType::UPDATE) &&
                   createFairMQTopo(_params.m_topologyFile) && changeStateConfigure();
    return createReturnValue(success, "Update done", "Update failed", measure.duration());
}

SReturnValue CControlService::SImpl::execConfigure()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeStateConfigure();
    return createReturnValue(success, "ConfigureRun done", "ConfigureRun failed", measure.duration());
}

SReturnValue CControlService::SImpl::execStart()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::Run);
    return createReturnValue(success, "Start done", "Start failed", measure.duration());
}

SReturnValue CControlService::SImpl::execStop()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::Stop);
    return createReturnValue(success, "Stop done", "Stop failed", measure.duration());
}

SReturnValue CControlService::SImpl::execReset()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeStateReset();
    return createReturnValue(success, "Reset done", "Reset failed", measure.duration());
}

SReturnValue CControlService::SImpl::execTerminate()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::End);
    return createReturnValue(success, "Terminate done", "Terminate failed", measure.duration());
}

SReturnValue CControlService::SImpl::execShutdown()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = shutdownDDSSession();
    return createReturnValue(success, "Shutdown done", "Shutdown failed", measure.duration());
}

SReturnValue CControlService::SImpl::createReturnValue(bool _success,
                                                       const std::string& _msg,
                                                       const std::string& _errMsg,
                                                       size_t _execTime)
{
    if (_success)
    {
        return SReturnValue(EStatusCode::ok, _msg, _execTime, SError(), m_runID);
    }
    return SReturnValue(EStatusCode::error, "", _execTime, SError(123, _errMsg), m_runID);
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
    std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(m_timeout));

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

bool CControlService::SImpl::waitForNumActiveAgents(size_t _numAgents)
{
    try
    {
        m_session->waitForNumAgents<CSession::EAgentState::active>(
            _numAgents, std::chrono::seconds(m_timeout), std::chrono::milliseconds(500), 3600);
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
    std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(m_timeout));

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

bool CControlService::SImpl::changeState(fair::mq::sdk::TopologyTransition _transition)
{
    if (m_fairmqTopology == nullptr)
        return false;

    bool success(true);

    try
    {
        fair::mq::sdk::DeviceState targetState;
        std::condition_variable cv;

        m_fairmqTopology->AsyncChangeState(
            _transition,
            std::chrono::seconds(m_timeout),
            [&cv, &success, &targetState](std::error_code _ec, fair::mq::sdk::TopologyState _state) {
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
                cv.notify_all();
            });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(m_timeout));

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

bool CControlService::SImpl::changeStateConfigure()
{
    return changeState(fair::mq::sdk::TopologyTransition::InitDevice) &&
           changeState(fair::mq::sdk::TopologyTransition::CompleteInit) &&
           changeState(fair::mq::sdk::TopologyTransition::Bind) &&
           changeState(fair::mq::sdk::TopologyTransition::Connect) &&
           changeState(fair::mq::sdk::TopologyTransition::InitTask);
}

bool CControlService::SImpl::changeStateReset()
{
    return changeState(fair::mq::sdk::TopologyTransition::ResetTask) &&
           changeState(fair::mq::sdk::TopologyTransition::ResetDevice);
}

//
// CControlService
//

CControlService::CControlService()
    : m_impl(make_shared<CControlService::SImpl>())
{
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

SReturnValue CControlService::execConfigure()
{
    return m_impl->execConfigure();
}

SReturnValue CControlService::execStart()
{
    return m_impl->execStart();
}

SReturnValue CControlService::execStop()
{
    return m_impl->execStop();
}

SReturnValue CControlService::execReset()
{
    return m_impl->execReset();
}

SReturnValue CControlService::execTerminate()
{
    return m_impl->execTerminate();
}

SReturnValue CControlService::execShutdown()
{
    return m_impl->execShutdown();
}
