// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "GrpcControlClient.h"

using odc::ActivateRequest;
using odc::ConfigureRequest;
using odc::GeneralReply;
using odc::InitializeRequest;
using odc::ODC;
using odc::ReplyStatus;
using odc::ResetRequest;
using odc::ShutdownRequest;
using odc::StartRequest;
using odc::StopRequest;
using odc::SubmitRequest;
using odc::TerminateRequest;
using odc::UpdateRequest;

using namespace odc::core;
using namespace std;

CGrpcControlClient::CGrpcControlClient(shared_ptr<grpc::Channel> channel)
    : m_stub(ODC::NewStub(channel))
{
}

std::string CGrpcControlClient::requestInitialize(const SInitializeParams& _params)
{
    InitializeRequest request;
    request.set_runid(_params.m_runID);
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Initialize(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestSubmit(const SSubmitParams& _params)
{
    // Submit parameters are not used for the request.

    SubmitRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Submit(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestActivate(const SActivateParams& _params)
{
    ActivateRequest request;
    request.set_topology(_params.m_topologyFile);
    GeneralReply reply;
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

std::string CGrpcControlClient::requestConfigure()
{
    ConfigureRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Configure(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestStart()
{
    StartRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Start(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestStop()
{
    StopRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Stop(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestReset()
{
    ResetRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Reset(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestTerminate()
{
    TerminateRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Terminate(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestShutdown()
{
    ShutdownRequest request;
    GeneralReply reply;
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
        ss << "RPC failed with error code " << _status.error_code() << ": " << _status.error_message() << std::endl;
        return ss.str();
    }
}

std::string CGrpcControlClient::updateRequest(const SUpdateParams& _params)
{
    UpdateRequest request;
    request.set_topology(_params.m_topologyFile);
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Update(&context, request, &reply);
    return GetReplyString(status, reply);
}
