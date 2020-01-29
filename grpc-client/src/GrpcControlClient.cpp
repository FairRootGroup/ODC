// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "GrpcControlClient.h"

using odc::ConfigureRunRequest;
using odc::GeneralReply;
using odc::InitializeRequest;
using odc::ODC;
using odc::ReplyStatus;
using odc::ShutdownRequest;
using odc::StartRequest;
using odc::StopRequest;
using odc::TerminateRequest;
using odc::UpdateTopologyRequest;

CGrpcControlClient::CGrpcControlClient(std::shared_ptr<grpc::Channel> channel)
    : m_stub(ODC::NewStub(channel))
{
}

std::string CGrpcControlClient::RequestInitialize(const std::string& _topo)
{
    InitializeRequest request;
    request.set_topology(_topo);
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Initialize(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::RequestConfigureRun()
{
    ConfigureRunRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->ConfigureRun(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::RequestStart()
{
    StartRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Start(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::RequestStop()
{
    StopRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Stop(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::RequestTerminate()
{
    TerminateRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Terminate(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::RequestShutdown()
{
    ShutdownRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Shutdown(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::RequestUpdateTopology(const std::string& _topo)
{
    UpdateTopologyRequest request;
    request.set_topology(_topo);
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->UpdateTopology(&context, request, &reply);
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
