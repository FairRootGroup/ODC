// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "GrpcControlClient.h"

using namespace odc::core;
using namespace std;

CGrpcControlClient::CGrpcControlClient(shared_ptr<grpc::Channel> channel)
    : m_stub(odc::ODC::NewStub(channel))
{
}

std::string CGrpcControlClient::requestInitialize(const SInitializeParams& _params)
{
    odc::InitializeRequest request;
    request.set_runid(_params.m_runID);
    request.set_sessionid(_params.m_sessionID);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Initialize(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestSubmit(const SSubmitParams& _params)
{
    // Submit parameters are not used for the request.

    odc::SubmitRequest request;
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Submit(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestActivate(const SActivateParams& _params)
{
    odc::ActivateRequest request;
    request.set_topology(_params.m_topologyFile);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Activate(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestUpscale(const SUpdateParams& _params)
{
    return updateRequest(_params);
}

std::string CGrpcControlClient::requestDownscale(const SUpdateParams& _params)
{
    return updateRequest(_params);
}

std::string CGrpcControlClient::requestConfigure(const SDeviceParams& _params)
{
    return stateChangeRequest<odc::ConfigureRequest>(_params, &odc::ODC::Stub::Configure);
}

std::string CGrpcControlClient::requestStart(const SDeviceParams& _params)
{
    return stateChangeRequest<odc::StartRequest>(_params, &odc::ODC::Stub::Start);
}

std::string CGrpcControlClient::requestStop(const SDeviceParams& _params)
{
    return stateChangeRequest<odc::StopRequest>(_params, &odc::ODC::Stub::Stop);
}

std::string CGrpcControlClient::requestReset(const SDeviceParams& _params)
{
    return stateChangeRequest<odc::ResetRequest>(_params, &odc::ODC::Stub::Reset);
}

std::string CGrpcControlClient::requestTerminate(const SDeviceParams& _params)
{
    return stateChangeRequest<odc::TerminateRequest>(_params, &odc::ODC::Stub::Terminate);
}

std::string CGrpcControlClient::requestShutdown()
{
    odc::ShutdownRequest request;
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Shutdown(&context, request, &reply);
    return GetReplyString(status, reply);
}

template <typename Reply_t>
std::string CGrpcControlClient::GetReplyString(const grpc::Status& _status, const Reply_t& _reply)
{
    if (_status.ok())
    {
        return _reply.DebugString();
    }
    else
    {
        std::stringstream ss;
        ss << "RPC failed with error code " << _status.error_code() << ": " << _status.error_message() << endl;
        return ss.str();
    }
}

std::string CGrpcControlClient::updateRequest(const SUpdateParams& _params)
{
    odc::UpdateRequest request;
    request.set_topology(_params.m_topologyFile);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Update(&context, request, &reply);
    return GetReplyString(status, reply);
}

template <typename Request_t, typename StubFunc_t>
std::string CGrpcControlClient::stateChangeRequest(const SDeviceParams& _params, StubFunc_t _stubFunc)
{
    // Protobuf message takes the ownership and deletes the object
    odc::StateChangeRequest* stateChange = new odc::StateChangeRequest();
    stateChange->set_path(_params.m_path);
    stateChange->set_detailed(_params.m_detailed);

    Request_t request;
    request.set_allocated_request(stateChange);

    odc::StateChangeReply reply;
    grpc::ClientContext context;
    grpc::Status status = (m_stub.get()->*_stubFunc)(&context, request, &reply);
    return GetReplyString(status, reply);
}
