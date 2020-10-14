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

std::string CGrpcControlClient::requestInitialize(odc::core::partitionID_t _partitionID,
                                                  const SInitializeParams& _params)
{
    odc::InitializeRequest request;
    request.set_partitionid(_partitionID);
    request.set_sessionid(_params.m_sessionID);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Initialize(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestSubmit(odc::core::partitionID_t _partitionID, const SSubmitParams& _params)
{
    // Submit parameters are not used for the request.

    odc::SubmitRequest request;
    request.set_partitionid(_partitionID);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Submit(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestActivate(odc::core::partitionID_t _partitionID, const SActivateParams& _params)
{
    odc::ActivateRequest request;
    request.set_partitionid(_partitionID);
    request.set_topology(_params.m_topologyFile);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Activate(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestRun(odc::core::partitionID_t _partitionID,
                                           const odc::core::SInitializeParams& _initializeParams,
                                           const odc::core::SSubmitParams& _submitParams,
                                           const odc::core::SActivateParams& _activateParams)
{
    odc::RunRequest request;
    request.set_partitionid(_partitionID);
    request.set_topology(_activateParams.m_topologyFile);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Run(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestUpscale(odc::core::partitionID_t _partitionID, const SUpdateParams& _params)
{
    return updateRequest(_partitionID, _params);
}

std::string CGrpcControlClient::requestDownscale(odc::core::partitionID_t _partitionID, const SUpdateParams& _params)
{
    return updateRequest(_partitionID, _params);
}

std::string CGrpcControlClient::requestGetState(odc::core::partitionID_t _partitionID,
                                                const odc::core::SDeviceParams& _params)
{
    odc::StateRequest request;
    request.set_partitionid(_partitionID);
    request.set_path(_params.m_path);
    request.set_detailed(_params.m_detailed);
    odc::StateReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->GetState(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestSetProperties(odc::core::partitionID_t _partitionID,
                                                     const odc::core::SSetPropertiesParams& _params)
{
    odc::SetPropertiesRequest request;
    request.set_partitionid(_partitionID);
    request.set_path(_params.m_path);
    for (const auto& v : _params.m_properties)
    {
        auto prop = request.add_properties();
        prop->set_key(v.first);
        prop->set_value(v.second);
    }
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->SetProperties(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestConfigure(odc::core::partitionID_t _partitionID, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::ConfigureRequest>(_partitionID, _params, &odc::ODC::Stub::Configure);
}

std::string CGrpcControlClient::requestStart(odc::core::partitionID_t _partitionID, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::StartRequest>(_partitionID, _params, &odc::ODC::Stub::Start);
}

std::string CGrpcControlClient::requestStop(odc::core::partitionID_t _partitionID, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::StopRequest>(_partitionID, _params, &odc::ODC::Stub::Stop);
}

std::string CGrpcControlClient::requestReset(odc::core::partitionID_t _partitionID, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::ResetRequest>(_partitionID, _params, &odc::ODC::Stub::Reset);
}

std::string CGrpcControlClient::requestTerminate(odc::core::partitionID_t _partitionID, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::TerminateRequest>(_partitionID, _params, &odc::ODC::Stub::Terminate);
}

std::string CGrpcControlClient::requestShutdown(odc::core::partitionID_t _partitionID)
{
    odc::ShutdownRequest request;
    request.set_partitionid(_partitionID);
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

std::string CGrpcControlClient::updateRequest(odc::core::partitionID_t _partitionID, const SUpdateParams& _params)
{
    odc::UpdateRequest request;
    request.set_partitionid(_partitionID);
    request.set_topology(_params.m_topologyFile);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Update(&context, request, &reply);
    return GetReplyString(status, reply);
}

template <typename Request_t, typename StubFunc_t>
std::string CGrpcControlClient::stateChangeRequest(odc::core::partitionID_t _partitionID,
                                                   const SDeviceParams& _params,
                                                   StubFunc_t _stubFunc)
{
    // Protobuf message takes the ownership and deletes the object
    odc::StateRequest* stateChange = new odc::StateRequest();
    stateChange->set_partitionid(_partitionID);
    stateChange->set_path(_params.m_path);
    stateChange->set_detailed(_params.m_detailed);

    Request_t request;
    request.set_allocated_request(stateChange);

    odc::StateReply reply;
    grpc::ClientContext context;
    grpc::Status status = (m_stub.get()->*_stubFunc)(&context, request, &reply);
    return GetReplyString(status, reply);
}
