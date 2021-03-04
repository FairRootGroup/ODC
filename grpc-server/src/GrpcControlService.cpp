// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcControlService.h"

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

CGrpcControlService::CGrpcControlService()
    : m_service(make_shared<CControlService>())
{
}

void CGrpcControlService::setTimeout(const std::chrono::seconds& _timeout)
{
    m_service->setTimeout(_timeout);
}

void CGrpcControlService::registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap)
{
    m_service->registerResourcePlugins(_pluginMap);
}

::grpc::Status CGrpcControlService::Initialize(::grpc::ServerContext* context,
                                               const odc::InitializeRequest* request,
                                               odc::GeneralReply* response)
{
    SInitializeParams params{ request->sessionid() };
    SReturnValue value = m_service->execInitialize(request->partitionid(), params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Submit(::grpc::ServerContext* context,
                                           const odc::SubmitRequest* request,
                                           odc::GeneralReply* response)
{
    SSubmitParams params{ request->plugin(), request->resources() };
    SReturnValue value = m_service->execSubmit(request->partitionid(), params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Activate(::grpc::ServerContext* context,
                                             const odc::ActivateRequest* request,
                                             odc::GeneralReply* response)
{
    SActivateParams params{ request->topology() };
    SReturnValue value = m_service->execActivate(request->partitionid(), params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Run(::grpc::ServerContext* context,
                                        const odc::RunRequest* request,
                                        odc::GeneralReply* response)
{
    SInitializeParams initializeParams{ "" };
    SSubmitParams submitParams{ request->plugin(), request->resources() };
    SActivateParams activateParams{ request->topology() };
    SReturnValue value = m_service->execRun(request->partitionid(), initializeParams, submitParams, activateParams);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Update(::grpc::ServerContext* context,
                                           const odc::UpdateRequest* request,
                                           odc::GeneralReply* response)
{
    SUpdateParams params{ request->topology() };
    SReturnValue value = m_service->execUpdate(request->partitionid(), params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::GetState(::grpc::ServerContext* context,
                                             const odc::StateRequest* request,
                                             odc::StateReply* response)
{
    SDeviceParams params{ request->path(), request->detailed() };
    SReturnValue value = m_service->execGetState(request->partitionid(), params);
    setupStateReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::SetProperties(::grpc::ServerContext* context,
                                                  const odc::SetPropertiesRequest* request,
                                                  odc::GeneralReply* response)
{
    // Convert from protobuf to ODC format
    SSetPropertiesParams::Properties_t props;
    for (size_t i = 0; i < request->properties_size(); i++)
    {
        auto prop{ request->properties(i) };
        props.push_back(SSetPropertiesParams::Property_t(prop.key(), prop.value()));
    }

    SSetPropertiesParams params{ props, request->path() };
    SReturnValue value = m_service->execSetProperties(request->partitionid(), params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Configure(::grpc::ServerContext* context,
                                              const odc::ConfigureRequest* request,
                                              odc::StateReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execConfigure(request->request().partitionid(), params);
    setupStateReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Start(::grpc::ServerContext* context,
                                          const odc::StartRequest* request,
                                          odc::StateReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execStart(request->request().partitionid(), params);
    setupStateReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Stop(::grpc::ServerContext* context,
                                         const odc::StopRequest* request,
                                         odc::StateReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execStop(request->request().partitionid(), params);
    setupStateReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Reset(::grpc::ServerContext* context,
                                          const odc::ResetRequest* request,
                                          odc::StateReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execReset(request->request().partitionid(), params);
    setupStateReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Terminate(::grpc::ServerContext* context,
                                              const odc::TerminateRequest* request,
                                              odc::StateReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execTerminate(request->request().partitionid(), params);
    setupStateReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Shutdown(::grpc::ServerContext* context,
                                             const odc::ShutdownRequest* request,
                                             odc::GeneralReply* response)
{
    SReturnValue value = m_service->execShutdown(request->partitionid());
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

void CGrpcControlService::setupGeneralReply(odc::GeneralReply* _response, const SReturnValue& _value)
{
    if (_value.m_statusCode == EStatusCode::ok)
    {
        _response->set_status(odc::ReplyStatus::SUCCESS);
        _response->set_msg(_value.m_msg);
    }
    else
    {
        _response->set_status(odc::ReplyStatus::ERROR);
        //_response->set_msg("");
        // Protobuf message takes the ownership and deletes the object
        odc::Error* error = new odc::Error();
        error->set_code(_value.m_error.m_code.value());
        error->set_msg(_value.m_error.m_code.message() + " (" + _value.m_error.m_details + ")");
        _response->set_allocated_error(error);
    }
    _response->set_partitionid(_value.m_partitionID);
    _response->set_sessionid(_value.m_sessionID);
    _response->set_exectime(_value.m_execTime);
    _response->set_state(GetAggregatedTopologyStateName(_value.m_aggregatedState));
}

void CGrpcControlService::setupStateReply(odc::StateReply* _response, const odc::core::SReturnValue& _value)
{
    // Protobuf message takes the ownership and deletes the object
    odc::GeneralReply* generalResponse = new odc::GeneralReply();
    setupGeneralReply(generalResponse, _value);
    _response->set_allocated_reply(generalResponse);

    if (_value.m_details != nullptr)
    {
        const auto& topologyState = _value.m_details->m_topologyState;
        for (const auto& state : topologyState)
        {
            auto device = _response->add_devices();
            device->set_path(state.m_path);
            device->set_id(state.m_status.taskId);
            device->set_state(fair::mq::GetStateName(state.m_status.state));
        }
    }
}
