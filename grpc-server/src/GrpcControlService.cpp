// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcControlService.h"

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

GrpcControlService::GrpcControlService(const odc::core::ControlService::SConfigParams& _params)
    : m_service(make_shared<ControlService>(_params))
{
}

::grpc::Status GrpcControlService::Initialize(::grpc::ServerContext* context,
                                              const odc::InitializeRequest* request,
                                              odc::GeneralReply* response)
{
    SReturnValue value = m_service->Initialize();
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status GrpcControlService::ConfigureRun(::grpc::ServerContext* context,
                                                const odc::ConfigureRunRequest* request,
                                                odc::GeneralReply* response)
{
    SReturnValue value = m_service->ConfigureRun();
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status GrpcControlService::Start(::grpc::ServerContext* context,
                                         const odc::StartRequest* request,
                                         odc::GeneralReply* response)
{
    SReturnValue value = m_service->Start();
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status GrpcControlService::Stop(::grpc::ServerContext* context,
                                        const odc::StopRequest* request,
                                        odc::GeneralReply* response)
{
    SReturnValue value = m_service->Stop();
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status GrpcControlService::Terminate(::grpc::ServerContext* context,
                                             const odc::TerminateRequest* request,
                                             odc::GeneralReply* response)
{
    SReturnValue value = m_service->Terminate();
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

::grpc::Status GrpcControlService::Shutdown(::grpc::ServerContext* context,
                                            const odc::ShutdownRequest* request,
                                            odc::GeneralReply* response)
{
    SReturnValue value = m_service->Shutdown();
    setupGeneralReply(response, value);
    return ::grpc::Status::OK;
}

void GrpcControlService::setupGeneralReply(odc::GeneralReply* _response, const SReturnValue& _value)
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
        // protobuf will take care of deleting the object
        odc::Error* error = new odc::Error();
        error->set_code(_value.m_error.m_code);
        error->set_msg(_value.m_error.m_msg);
        _response->set_allocated_error(error);
    }
    _response->set_exectime(_value.m_execTime);
}
