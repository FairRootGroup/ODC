// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "DDSControlService.h"
// FairMQ
#include <fairmq/sdk/Topology.h>
// DDS control
#include "TimeMeasure.h"

using namespace ddscontrol;
using namespace std;
using namespace dds;
using namespace dds::tools_api;
using namespace dds::topology_api;

DDSControlService::DDSControlService(const SConfigParams& _params)
    : m_topo(nullptr)
    , m_session(make_shared<CSession>())
    , m_fairmqTopo(nullptr)
    , m_timeout(1800.)
    , m_configParams(_params)
{
}

grpc::Status DDSControlService::Initialize(grpc::ServerContext* context,
                                           const ddscontrol::InitializeRequest* request,
                                           ddscontrol::GeneralReply* response)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    bool success(true);

    string topologyFile = request->topology();
    std::pair<size_t, size_t> numAgents;
    try
    {
        m_topo = make_shared<CTopology>(topologyFile);
        numAgents = m_topo->getRequiredNofAgents(12);
    }
    catch (exception& _e)
    {
        success = false;
        cerr << "Failed to initialize topology: " << _e.what() << endl;
    }

    if (success)
    {
        // Shut down DDS session if it is running already
        // Create new DDS session
        // Submit agents
        // Activate the topology
        size_t allCount = numAgents.first * numAgents.second;
        success = shutdownDDSSession() && createDDSSession() && submitDDSAgents(numAgents.first, numAgents.second) &&
                  waitForNumActiveAgents(allCount) && activateDDSTopology(topologyFile);
    }

    if (m_fairmqTopo == nullptr && success)
    {
        try
        {
            fair::mq::sdk::DDSEnv env;
            fair::mq::sdk::DDSTopo topo(*m_topo, env);
            fair::mq::sdk::DDSSession session(m_session, env);
            m_fairmqTopo = make_shared<fair::mq::sdk::Topology>(std::move(topo), std::move(session));

            // TODO Use this instead of the above once FairMQ repaired the move ctor of sdk::Topology
            // m_fairmqTopo = make_shared<fair::mq::sdk::Topology>(fair::mq::sdk::MakeTopology(*m_topo, m_session));
        }
        catch (exception& _e)
        {
            success = false;
            cerr << "Failed to initialize FairMQ topology: " << _e.what() << endl;
        }
    }

    // Shutdown DDS session if operation failed
    if (!success)
    {
        shutdownDDSSession();
    }

    setupGeneralReply(response, success, "Initialize done", "Initialize failed", measure.duration());
    return grpc::Status::OK;
}

grpc::Status DDSControlService::ConfigureRun(grpc::ServerContext* context,
                                             const ddscontrol::ConfigureRunRequest* request,
                                             ddscontrol::GeneralReply* response)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    bool success = changeState(fair::mq::sdk::TopologyTransition::InitDevice) &&
                   changeState(fair::mq::sdk::TopologyTransition::CompleteInit) &&
                   changeState(fair::mq::sdk::TopologyTransition::Bind) &&
                   changeState(fair::mq::sdk::TopologyTransition::Connect) &&
                   changeState(fair::mq::sdk::TopologyTransition::InitTask);

    setupGeneralReply(response, success, "ConfigureRun done", "ConfigureRun failed", measure.duration());

    return grpc::Status::OK;
}

grpc::Status DDSControlService::Start(grpc::ServerContext* context,
                                      const ddscontrol::StartRequest* request,
                                      ddscontrol::GeneralReply* response)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::Run);
    setupGeneralReply(response, success, "Start done", "Start failed", measure.duration());
    return grpc::Status::OK;
}

grpc::Status DDSControlService::Stop(grpc::ServerContext* context,
                                     const ddscontrol::StopRequest* request,
                                     ddscontrol::GeneralReply* response)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::Stop);
    setupGeneralReply(response, success, "Stop done", "Stop failed", measure.duration());
    return grpc::Status::OK;
}

grpc::Status DDSControlService::Terminate(grpc::ServerContext* context,
                                          const ddscontrol::TerminateRequest* request,
                                          ddscontrol::GeneralReply* response)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = changeState(fair::mq::sdk::TopologyTransition::ResetTask) &&
                   changeState(fair::mq::sdk::TopologyTransition::ResetDevice) &&
                   changeState(fair::mq::sdk::TopologyTransition::End);
    setupGeneralReply(response, success, "Terminate done", "Terminate failed", measure.duration());
    return grpc::Status::OK;
}

grpc::Status DDSControlService::Shutdown(grpc::ServerContext* context,
                                         const ddscontrol::ShutdownRequest* request,
                                         ddscontrol::GeneralReply* response)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    bool success = shutdownDDSSession();
    setupGeneralReply(response, success, "Shutdown done", "Shutdown failed", measure.duration());
    return grpc::Status::OK;
}

bool DDSControlService::createDDSSession()
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

bool DDSControlService::submitDDSAgents(size_t _numAgents, size_t _numSlots)
{
    bool success(true);

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_rms = m_configParams.m_rmsPlugin;
    if (m_configParams.m_rmsPlugin == "localhost")
    {
        requestInfo.m_instances = _numAgents;
        requestInfo.m_slots = _numSlots;
    }
    else
    {
        requestInfo.m_instances = _numAgents;
        requestInfo.m_slots = _numSlots;
        requestInfo.m_config = m_configParams.m_configFile;
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

bool DDSControlService::waitForNumActiveAgents(size_t _numAgents)
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

bool DDSControlService::activateDDSTopology(const string& _topologyFile)
{
    bool success(true);

    STopologyRequest::request_t topoInfo;
    topoInfo.m_topologyFile = _topologyFile;
    topoInfo.m_disableValidation = true;
    topoInfo.m_updateType = STopologyRequest::request_t::EUpdateType::ACTIVATE;

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

bool DDSControlService::shutdownDDSSession()
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

void DDSControlService::setupGeneralReply(ddscontrol::GeneralReply* _response,
                                          bool _success,
                                          const std::string& _msg,
                                          const std::string& _errMsg,
                                          size_t _execTime)
{
    if (_success)
    {
        _response->set_status(ddscontrol::ReplyStatus::SUCCESS);
        _response->set_msg(_msg);
    }
    else
    {
        _response->set_status(ddscontrol::ReplyStatus::ERROR);
        //_response->set_msg("");
        // protobuf will take care of deleting the object
        ddscontrol::Error* error = new ddscontrol::Error();
        error->set_code(123);
        error->set_msg(_errMsg);
        _response->set_allocated_error(error);
    }
    _response->set_exectime(_execTime);
}

bool DDSControlService::changeState(fair::mq::sdk::TopologyTransition _transition)
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
