// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "DDSControlClient.h"

using ddscontrol::ConfigureRunRequest;
using ddscontrol::DDSControl;
using ddscontrol::GeneralReply;
using ddscontrol::InitializeRequest;
using ddscontrol::ReplyStatus;
using ddscontrol::StartRequest;
using ddscontrol::StopRequest;
using ddscontrol::TerminateRequest;

DDSControlClient::DDSControlClient(std::shared_ptr<grpc::Channel> channel)
    : m_stub(DDSControl::NewStub(channel))
{
}

std::string DDSControlClient::RequestInitialize()
{
    InitializeRequest request;
    request.set_numworkers(10);
    request.set_topofile("~/DDS-control/test/sample_topo.xml");
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Initialize(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string DDSControlClient::RequestConfigureRun()
{
    ConfigureRunRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->ConfigureRun(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string DDSControlClient::RequestStart()
{
    StartRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Start(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string DDSControlClient::RequestStop()
{
    StopRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Stop(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string DDSControlClient::RequestTerminate()
{
    TerminateRequest request;
    GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Terminate(&context, request, &reply);
    return GetReplyString(status, reply);
}

template <typename Reply_t>
std::string DDSControlClient::GetReplyString(const grpc::Status& _status, const Reply_t& _reply)
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
