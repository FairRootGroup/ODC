/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ODC
#include <odc/grpc/GrpcControlClient.h>

using namespace odc::core;
using namespace std;

CGrpcControlClient::CGrpcControlClient(shared_ptr<grpc::Channel> channel)
    : m_stub(odc::ODC::NewStub(channel))
{
}

std::string CGrpcControlClient::requestInitialize(const SCommonParams& _common, const SInitializeParams& _params)
{
    odc::InitializeRequest request;
    updateCommonParams(_common, &request);
    request.set_sessionid(_params.m_sessionID);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Initialize(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestSubmit(const SCommonParams& _common, const SSubmitParams& _params)
{
    // Submit parameters are not used for the request.

    odc::SubmitRequest request;
    updateCommonParams(_common, &request);
    request.set_plugin(_params.m_plugin);
    request.set_resources(_params.m_resources);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Submit(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestActivate(const SCommonParams& _common, const SActivateParams& _params)
{
    odc::ActivateRequest request;
    updateCommonParams(_common, &request);
    request.set_topology(_params.m_topologyFile);
    request.set_content(_params.m_topologyContent);
    request.set_script(_params.m_topologyScript);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Activate(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestRun(const SCommonParams& _common,
                                           const odc::core::SInitializeParams& /*_initializeParams*/,
                                           const odc::core::SSubmitParams& _submitParams,
                                           const odc::core::SActivateParams& _activateParams)
{
    odc::RunRequest request;
    updateCommonParams(_common, &request);
    request.set_plugin(_submitParams.m_plugin);
    request.set_resources(_submitParams.m_resources);
    request.set_topology(_activateParams.m_topologyFile);
    request.set_content(_activateParams.m_topologyContent);
    request.set_script(_activateParams.m_topologyScript);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Run(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestUpscale(const SCommonParams& _common, const SUpdateParams& _params)
{
    return updateRequest(_common, _params);
}

std::string CGrpcControlClient::requestDownscale(const SCommonParams& _common, const SUpdateParams& _params)
{
    return updateRequest(_common, _params);
}

std::string CGrpcControlClient::requestGetState(const SCommonParams& _common, const odc::core::SDeviceParams& _params)
{
    odc::StateRequest request;
    updateCommonParams(_common, &request);
    request.set_path(_params.m_path);
    request.set_detailed(_params.m_detailed);
    odc::StateReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->GetState(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestSetProperties(const SCommonParams& _common,
                                                     const odc::core::SSetPropertiesParams& _params)
{
    odc::SetPropertiesRequest request;
    updateCommonParams(_common, &request);
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

std::string CGrpcControlClient::requestConfigure(const SCommonParams& _common, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::ConfigureRequest>(_common, _params, &odc::ODC::Stub::Configure);
}

std::string CGrpcControlClient::requestStart(const SCommonParams& _common, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::StartRequest>(_common, _params, &odc::ODC::Stub::Start);
}

std::string CGrpcControlClient::requestStop(const SCommonParams& _common, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::StopRequest>(_common, _params, &odc::ODC::Stub::Stop);
}

std::string CGrpcControlClient::requestReset(const SCommonParams& _common, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::ResetRequest>(_common, _params, &odc::ODC::Stub::Reset);
}

std::string CGrpcControlClient::requestTerminate(const SCommonParams& _common, const SDeviceParams& _params)
{
    return stateChangeRequest<odc::TerminateRequest>(_common, _params, &odc::ODC::Stub::Terminate);
}

std::string CGrpcControlClient::requestShutdown(const SCommonParams& _common)
{
    odc::ShutdownRequest request;
    updateCommonParams(_common, &request);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Shutdown(&context, request, &reply);
    return GetReplyString(status, reply);
}

std::string CGrpcControlClient::requestStatus(const odc::core::SStatusParams& _params)
{
    odc::StatusRequest request;
    request.set_running(_params.m_running);
    odc::StatusReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Status(&context, request, &reply);
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

std::string CGrpcControlClient::updateRequest(const SCommonParams& _common, const SUpdateParams& _params)
{
    odc::UpdateRequest request;
    updateCommonParams(_common, &request);
    request.set_topology(_params.m_topologyFile);
    odc::GeneralReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->Update(&context, request, &reply);
    return GetReplyString(status, reply);
}

template <typename Request_t, typename StubFunc_t>
std::string CGrpcControlClient::stateChangeRequest(const SCommonParams& _common,
                                                   const SDeviceParams& _params,
                                                   StubFunc_t _stubFunc)
{
    // Protobuf message takes the ownership and deletes the object
    odc::StateRequest* stateChange = new odc::StateRequest();
    updateCommonParams(_common, stateChange);
    stateChange->set_path(_params.m_path);
    stateChange->set_detailed(_params.m_detailed);

    Request_t request;
    request.set_allocated_request(stateChange);

    odc::StateReply reply;
    grpc::ClientContext context;
    grpc::Status status = (m_stub.get()->*_stubFunc)(&context, request, &reply);
    return GetReplyString(status, reply);
}

template <typename Request_t>
void CGrpcControlClient::updateCommonParams(const odc::core::SCommonParams& _common, Request_t* _request)
{
    _request->set_partitionid(_common.m_partitionID);
    _request->set_runnr(_common.m_runNr);
    _request->set_timeout(_common.m_timeout);
}
