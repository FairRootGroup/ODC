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

void CGrpcControlService::setSubmitParams(const odc::core::SSubmitParams& _params)
{
    m_submitParams = _params;
}

::grpc::Status CGrpcControlService::Initialize(::grpc::ServerContext* context,
                                               const odc::InitializeRequest* request,
                                               odc::GeneralReply* response)
{
    SInitializeParams params{ request->runid(), request->sessionid() };
    SReturnValue value = m_service->execInitialize(params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Submit(::grpc::ServerContext* context,
                                           const odc::SubmitRequest* request,
                                           odc::GeneralReply* response)
{
    SReturnValue value = m_service->execSubmit(m_submitParams);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Activate(::grpc::ServerContext* context,
                                             const odc::ActivateRequest* request,
                                             odc::GeneralReply* response)
{
    SActivateParams params{ request->topology() };
    SReturnValue value = m_service->execActivate(params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Update(::grpc::ServerContext* context,
                                           const odc::UpdateRequest* request,
                                           odc::GeneralReply* response)
{
    SUpdateParams params{ request->topology() };
    SReturnValue value = m_service->execUpdate(params);
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Configure(::grpc::ServerContext* context,
                                              const odc::ConfigureRequest* request,
                                              odc::StateChangeReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execConfigure(params);
    setupStateChangeReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Start(::grpc::ServerContext* context,
                                          const odc::StartRequest* request,
                                          odc::StateChangeReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execStart(params);
    setupStateChangeReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Stop(::grpc::ServerContext* context,
                                         const odc::StopRequest* request,
                                         odc::StateChangeReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execStop(params);
    setupStateChangeReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Reset(::grpc::ServerContext* context,
                                          const odc::ResetRequest* request,
                                          odc::StateChangeReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execReset(params);
    setupStateChangeReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Terminate(::grpc::ServerContext* context,
                                              const odc::TerminateRequest* request,
                                              odc::StateChangeReply* response)
{
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value = m_service->execTerminate(params);
    setupStateChangeReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcControlService::Shutdown(::grpc::ServerContext* context,
                                             const odc::ShutdownRequest* request,
                                             odc::GeneralReply* response)
{
    SReturnValue value = m_service->execShutdown();
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
        error->set_code(_value.m_error.m_code);
        error->set_msg(_value.m_error.m_msg);
        _response->set_allocated_error(error);
    }
    _response->set_runid(_value.m_runID);
    _response->set_sessionid(_value.m_sessionID);
    _response->set_exectime(_value.m_execTime);
}

void CGrpcControlService::setupStateChangeReply(odc::StateChangeReply* _response, const odc::core::SReturnValue& _value)
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
