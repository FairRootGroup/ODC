// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "DDSControlService.h"

using namespace std;
using namespace dds;
using namespace dds::tools_api;

// grpc::Status DDSControlService::Initialize(grpc::ServerContext* context, const ddscontrol::InitializeRequest*
// request, ddscontrol::GeneralReply* reply) override
//{
//    m_DDSCustomCmd.send(std::to_string(request->state()), "");
//
//    std::unique_lock<std::mutex> lock(m_changeStateMutex);
//    std::cv_status waitStatus = m_changeStateCondition.wait_for(lock, std::chrono::seconds(10));
//
//    if (waitStatus == std::cv_status::timeout)
//    {
//        std::cout << "Timout waiting for reply" << std::endl;
//
//        reply->set_state(ddscontrol::State::UNKNOWN);
//        // reply->set_allocated_error()
//    }
//    else
//    {
//        std::cout << "Got reply" << std::endl;
//        reply->set_state(request->state());
//    }
//
//    return Status::OK;
//}

grpc::Status DDSControlService::Initialize(grpc::ServerContext* context,
                                           const ddscontrol::InitializeRequest* request,
                                           ddscontrol::GeneralReply* response)
{
    size_t numAgents = request->numworkers();
    string topologyFile = request->topofile();

    createDDSSession();
    submitDDSAgents(numAgents);
    activateDDSTopology(topologyFile);

    response->set_status(ddscontrol::ReplyStatus::SUCCESS);
    response->set_msg("Initialize finished");

    return grpc::Status::OK;
}

grpc::Status DDSControlService::ConfigureRun(grpc::ServerContext* context,
                                             const ddscontrol::ConfigureRunRequest* request,
                                             ddscontrol::GeneralReply* response)
{
    response->set_status(ddscontrol::ReplyStatus::SUCCESS);
    response->set_msg("ConfigureRun finished");
    return grpc::Status::OK;
}

grpc::Status DDSControlService::Start(grpc::ServerContext* context,
                                      const ddscontrol::StartRequest* request,
                                      ddscontrol::GeneralReply* response)
{
    response->set_status(ddscontrol::ReplyStatus::SUCCESS);
    response->set_msg("Start finished");
    return grpc::Status::OK;
}

grpc::Status DDSControlService::Stop(grpc::ServerContext* context,
                                     const ddscontrol::StopRequest* request,
                                     ddscontrol::GeneralReply* response)
{
    response->set_status(ddscontrol::ReplyStatus::SUCCESS);
    response->set_msg("Stop finished");
    return grpc::Status::OK;
}

grpc::Status DDSControlService::Terminate(grpc::ServerContext* context,
                                          const ddscontrol::TerminateRequest* request,
                                          ddscontrol::GeneralReply* response)
{
    shutdownDDSSession();

    response->set_status(ddscontrol::ReplyStatus::SUCCESS);
    response->set_msg("Terminate finished");
    return grpc::Status::OK;
}

void DDSControlService::createDDSSession()
{
    boost::uuids::uuid sessionID = m_session.create();
}

void DDSControlService::submitDDSAgents(size_t _numAgents)
{
    std::condition_variable cv;
    std::mutex mtx;

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_config = "";
    requestInfo.m_rms = "localhost";
    requestInfo.m_instances = _numAgents;
    requestInfo.m_pluginPath = "";
    SSubmitRequest::ptr_t requestPtr = SSubmitRequest::makeRequest(requestInfo);

    requestPtr->setMessageCallback([](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
        {
            cerr << "Server reports error: " << _message.m_msg << endl;
        }
        else
        {
            cout << "Server reports: " << _message.m_msg;
        }
    });

    requestPtr->setDoneCallback([&cv]() {
        cout << "Agent submission done" << endl;
        cv.notify_all();
    });

    m_session.sendRequest<SSubmitRequest>(requestPtr);

    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(10));

    if (waitStatus == std::cv_status::timeout)
    {
        // TODO: FIXME: error reply
    }
    else
    {
        // TODO: FIXME: success reply
    }
}

void DDSControlService::activateDDSTopology(const string& _topologyFile)
{
    std::condition_variable cv;
    std::mutex mtx;

    STopologyRequest::request_t topoInfo;

    topoInfo.m_topologyFile = _topologyFile;
    topoInfo.m_disableValidation = false;
    topoInfo.m_updateType = STopologyRequest::request_t::EUpdateType::ACTIVATE;

    STopologyRequest::ptr_t requestPtr = STopologyRequest::makeRequest(topoInfo);

    requestPtr->setMessageCallback([](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error)
        {
            cerr << "Server reports error: " << _message.m_msg << endl;
        }
        else
        {
            cout << "Server reports: " << _message.m_msg;
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

    m_session.sendRequest<STopologyRequest>(requestPtr);

    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, std::chrono::seconds(10));

    if (waitStatus == std::cv_status::timeout)
    {
        // TODO: FIXME: error reply
    }
    else
    {
        // TODO: FIXME: success reply
    }
}

void DDSControlService::shutdownDDSSession()
{
    m_session.shutdown();
}
