// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "ControlService.h"
// FairMQ
#include <fairmq/sdk/Topology.h>
// DDS control
#include "TimeMeasure.h"

using namespace odc;
using namespace odc::core;
using namespace std;
using namespace dds;
using namespace dds::tools_api;
using namespace dds::topology_api;

CControlService::CControlService()
    : m_topo(nullptr)
    , m_session(make_shared<CSession>())
    , m_fairmqTopo(nullptr)
    , m_timeout(1800.)
{
//    fair::Logger::SetConsoleSeverity("debug");
}

SReturnValue CControlService::Initialize(const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    bool success(true);

    string topologyFile = _params.m_topologyFile;
    m_topo = createDDSTopo(topologyFile);
    success = m_topo != nullptr;

    if (success)
    {
        // Shut down DDS session if it is running already
        // Create new DDS session
        // Submit agents
        // Activate the topology
        std::pair<size_t, size_t> numAgents(m_topo->getRequiredNofAgents(12));
        size_t allCount = numAgents.first * numAgents.second;
        success = shutdownDDSSession() && createDDSSession() &&
                  submitDDSAgents(_params.m_rmsPlugin, _params.m_configFile, numAgents.first, numAgents.second) &&
                  waitForNumActiveAgents(allCount) && activateDDSTopology(topologyFile, STopologyRequest::request_t::EUpdateType::ACTIVATE);
    }

    if (success)
    {
        createFairMQTopo(m_topo);
        success = m_fairmqTopo != nullptr;
    }

    return createReturnValue(success, "Initialize done", "Initialize failed", measure.duration());
}

SReturnValue CControlService::ConfigureRun()
{
    STimeMeasure<std::chrono::milliseconds> measure;

    bool success = changeState(fair::mq::sdk::TopologyTransition::InitDevice) &&
                   changeState(fair::mq::sdk::TopologyTransition::CompleteInit) &&
                   changeState(fair::mq::sdk::TopologyTransition::Bind) &&
                   changeState(fair::mq::sdk::TopologyTransition::Connect) &&
                   changeState(fair::mq::sdk::TopologyTransition::InitTask);

    return createReturnValue(success, "ConfigureRun done", "ConfigureRun failed", measure.duration());
}

SReturnValue CControlService::Start()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::Run);
    return createReturnValue(success, "Start done", "Start failed", measure.duration());
}

SReturnValue CControlService::Stop()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::Stop);
    return createReturnValue(success, "Stop done", "Stop failed", measure.duration());
}

SReturnValue CControlService::Terminate()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::ResetTask) &&
                   changeState(fair::mq::sdk::TopologyTransition::ResetDevice) &&
                   changeState(fair::mq::sdk::TopologyTransition::End);
    return createReturnValue(success, "Terminate done", "Terminate failed", measure.duration());
}

SReturnValue CControlService::Shutdown()
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = shutdownDDSSession();
    return createReturnValue(success, "Shutdown done", "Shutdown failed", measure.duration());
}

SReturnValue CControlService::UpdateTopology(const SUpdateTopologyParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Change state of all devices to Stop
    bool success = changeState(fair::mq::sdk::TopologyTransition::Stop) &&
                   changeState(fair::mq::sdk::TopologyTransition::ResetTask) &&
                   changeState(fair::mq::sdk::TopologyTransition::ResetDevice);
    // New DDS topology object
    string topologyFile = _params.m_topologyFile;
    if (success) {
        cout << "Creating new DDS topology" << endl;
        m_topo = createDDSTopo(topologyFile);
        success = m_topo != nullptr;
    }
    // Activate new topology
    if (success) {
        cout << "Activating topology" << endl;
        activateDDSTopology(topologyFile, STopologyRequest::request_t::EUpdateType::UPDATE);
    }
    // New FairMQ topology object
    if (success)
    {
        cout << "Creating new FairMQ topology" << endl;
        createFairMQTopo(m_topo);
        success = m_fairmqTopo != nullptr;
    }
    // Reconfigure devices
    if (success) {
        cout << "Configuring FairMQ devices" << endl;
        success = changeState(fair::mq::sdk::TopologyTransition::InitDevice) &&
        changeState(fair::mq::sdk::TopologyTransition::CompleteInit) &&
        changeState(fair::mq::sdk::TopologyTransition::Bind) &&
        changeState(fair::mq::sdk::TopologyTransition::Connect) &&
        changeState(fair::mq::sdk::TopologyTransition::InitTask);
    }
    return createReturnValue(success, "Update topology done", "Update topology failed", measure.duration());
}

SReturnValue CControlService::createReturnValue(bool _success,
                                                const std::string& _msg,
                                                const std::string& _errMsg,
                                                size_t _execTime)
{
    if (_success)
    {
        return SReturnValue(EStatusCode::ok, _msg, _execTime, SError());
    }
    return SReturnValue(EStatusCode::error, "", _execTime, SError(123, _errMsg));
}

bool CControlService::createDDSSession()
{
    bool success(true);
    try
    {
        boost::uuids::uuid sessionID = m_session->create();
        cout << "DDS session created with session ID: " << to_string(sessionID) << endl;
    }
    catch (exception& _e)
    {
        success = false;
        cerr << "Failed to create DDS session: " << _e.what() << endl;
    }
    return success;
}

bool CControlService::submitDDSAgents(const string& _rmsPlugin,
                                      const string& _configFile,
                                      size_t _numAgents,
                                      size_t _numSlots)
{
    bool success(true);

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_rms = _rmsPlugin;
    if (_rmsPlugin == "localhost")
    {
        requestInfo.m_instances = _numAgents;
        requestInfo.m_slots = _numSlots;
    }
    else
    {
        requestInfo.m_instances = _numAgents;
        requestInfo.m_slots = _numSlots;
        requestInfo.m_config = _configFile;
    }

    std::condition_variable cv;

    SSubmitRequest::ptr_t requestPtr = SSubmitRequest::makeRequest(requestInfo);

    requestPtr->setMessageCallback([&success](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
        {
            success = false;
            cerr << "Server reports error: " << _message.m_msg << endl;
        }
        else
        {
            cout << "Server reports: " << _message.m_msg << endl;
        }
    });

    requestPtr->setDoneCallback([&cv]() {
        cout << "Agent submission done" << endl;
        cv.notify_all();
    });

    m_session->sendRequest<SSubmitRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(m_timeout));

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        cerr << "Timed out waiting for agent submission" << endl;
    }
    else
    {
        cout << "Agent submission done successfully" << endl;
    }
    return success;
}

bool CControlService::waitForNumActiveAgents(size_t _numAgents)
{
    try
    {
        m_session->waitForNumAgents<CSession::EAgentState::active>(
            _numAgents, std::chrono::seconds(m_timeout), std::chrono::milliseconds(500), 3600, &std::cout);
    }
    catch (std::exception& _e)
    {
        cerr << "Timeout waiting for DDS agents: " << _e.what() << endl;
        return false;
    }
    return true;
}

bool CControlService::activateDDSTopology(const string& _topologyFile, STopologyRequest::request_t::EUpdateType _updateType)
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
            cerr << "Server reports error: " << _message.m_msg << endl;
        }
        else
        {
            cout << "Server reports: " << _message.m_msg << endl;
        }
    });

    requestPtr->setProgressCallback([](const SProgressResponseData& _progress) {
        int completed = _progress.m_completed + _progress.m_errors;
        if (completed == _progress.m_total)
        {
            cout << "Activated tasks: " << _progress.m_completed << "\nErrors: " << _progress.m_errors
                 << "\nTotal: " << _progress.m_total << endl;
        }
    });

    requestPtr->setDoneCallback([&cv]() {
        cout << "Topology activation done" << endl;
        cv.notify_all();
    });

    m_session->sendRequest<STopologyRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(m_timeout));

    if (waitStatus == std::cv_status::timeout)
    {
        success = false;
        cerr << "Timed out waiting for agent submission" << endl;
    }
    else
    {
        cout << "Topology activation done successfully" << endl;
    }
    return success;
}

bool CControlService::shutdownDDSSession()
{
    bool success(true);
    try
    {
        if (m_session->IsRunning())
        {
            m_session->shutdown();
            if (m_session->getSessionID() == boost::uuids::nil_uuid())
            {
                cout << "DDS session shutted down" << endl;
            }
            else
            {
                cerr << "Failed to shut down DDS session" << endl;
                success = false;
            }
        }
    }
    catch (exception& _e)
    {
        success = false;
        cerr << "Shutdown failed: " << _e.what() << endl;
    }
    return success;
}

CControlService::DDSTopoPtr_t CControlService::createDDSTopo(const std::string& _topologyFile) const
{
    try
    {
        return make_shared<CTopology>(_topologyFile);
    }
    catch (exception& _e)
    {
        cerr << "Failed to initialize topology: " << _e.what() << endl;
    }
    return nullptr;
}

void CControlService::createFairMQTopo(const DDSTopoPtr_t& _topo)
{
    try
    {
        m_fairmqTopo.reset();
        fair::mq::sdk::DDSEnv env;
        fair::mq::sdk::DDSSession session(m_session, env);
        session.StopOnDestruction(false);
        fair::mq::sdk::DDSTopo topo(*_topo, env);
        m_fairmqTopo = make_shared<fair::mq::sdk::Topology>(topo, session);
    }
    catch (exception& _e)
    {
        m_fairmqTopo = nullptr;
        cerr << "Failed to initialize FairMQ topology: " << _e.what() << endl;
    }
}

bool CControlService::changeState(fair::mq::sdk::TopologyTransition _transition)
{
    if (m_fairmqTopo == nullptr)
        return false;

    bool success(true);

    try
    {
        fair::mq::sdk::DeviceState targetState;
        std::condition_variable cv;

        m_fairmqTopo->AsyncChangeState(
            _transition,
            std::chrono::seconds(m_timeout),
            [&cv, &success, &targetState](std::error_code _ec, fair::mq::sdk::TopologyState _state) {
                cout << "Change transition result: " << _ec.message() << endl;
                success = !_ec;
                try
                {
                    targetState = fair::mq::sdk::AggregateState(_state);
                }
                catch (exception& _e)
                {
                    success = false;
                    cerr << "Change state failed: " << _e.what() << endl;
                }
                cv.notify_all();
            });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(m_timeout));

        if (waitStatus == std::cv_status::timeout)
        {
            success = false;
            cerr << "Timed out waiting for change state " << _transition << endl;
        }
        else
        {
            cout << "Change state done successfully " << _transition << endl;
        }
    }
    catch (exception& _e)
    {
        success = false;
        cerr << "Change state failed: " << _e.what() << endl;
    }

    return success;
}
