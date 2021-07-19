// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcSyncService.h"
// gRPC
#include <grpcpp/grpcpp.h>

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

CGrpcSyncService::CGrpcSyncService()
    : m_service(make_shared<CGrpcService>())
{
}

void CGrpcSyncService::run(const std::string& _host)
{
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, ::grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    server->Wait();
}

void CGrpcSyncService::setTimeout(const std::chrono::seconds& _timeout)
{
    m_service->setTimeout(_timeout);
}

void CGrpcSyncService::registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap)
{
    m_service->registerResourcePlugins(_pluginMap);
}

void CGrpcSyncService::registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& _triggerMap)
{
    m_service->registerRequestTriggers(_triggerMap);
}

::grpc::Status CGrpcSyncService::Initialize(::grpc::ServerContext* context,
                                            const odc::InitializeRequest* request,
                                            odc::GeneralReply* response)
{
    return m_service->Initialize(context, request, response);
}

::grpc::Status CGrpcSyncService::Submit(::grpc::ServerContext* context,
                                        const odc::SubmitRequest* request,
                                        odc::GeneralReply* response)
{
    return m_service->Submit(context, request, response);
}

::grpc::Status CGrpcSyncService::Activate(::grpc::ServerContext* context,
                                          const odc::ActivateRequest* request,
                                          odc::GeneralReply* response)
{
    return m_service->Activate(context, request, response);
}

::grpc::Status CGrpcSyncService::Run(::grpc::ServerContext* context,
                                     const odc::RunRequest* request,
                                     odc::GeneralReply* response)
{
    return m_service->Run(context, request, response);
}

::grpc::Status CGrpcSyncService::Update(::grpc::ServerContext* context,
                                        const odc::UpdateRequest* request,
                                        odc::GeneralReply* response)
{
    return m_service->Update(context, request, response);
}

::grpc::Status CGrpcSyncService::GetState(::grpc::ServerContext* context,
                                          const odc::StateRequest* request,
                                          odc::StateReply* response)
{
    return m_service->GetState(context, request, response);
}

::grpc::Status CGrpcSyncService::SetProperties(::grpc::ServerContext* context,
                                               const odc::SetPropertiesRequest* request,
                                               odc::GeneralReply* response)
{
    return m_service->SetProperties(context, request, response);
}

::grpc::Status CGrpcSyncService::Configure(::grpc::ServerContext* context,
                                           const odc::ConfigureRequest* request,
                                           odc::StateReply* response)
{
    return m_service->Configure(context, request, response);
}

::grpc::Status CGrpcSyncService::Start(::grpc::ServerContext* context,
                                       const odc::StartRequest* request,
                                       odc::StateReply* response)
{
    return m_service->Start(context, request, response);
}

::grpc::Status CGrpcSyncService::Stop(::grpc::ServerContext* context,
                                      const odc::StopRequest* request,
                                      odc::StateReply* response)
{
    return m_service->Stop(context, request, response);
}

::grpc::Status CGrpcSyncService::Reset(::grpc::ServerContext* context,
                                       const odc::ResetRequest* request,
                                       odc::StateReply* response)
{
    return m_service->Reset(context, request, response);
}

::grpc::Status CGrpcSyncService::Terminate(::grpc::ServerContext* context,
                                           const odc::TerminateRequest* request,
                                           odc::StateReply* response)
{
    return m_service->Terminate(context, request, response);
}

::grpc::Status CGrpcSyncService::Shutdown(::grpc::ServerContext* context,
                                          const odc::ShutdownRequest* request,
                                          odc::GeneralReply* response)
{
    return m_service->Shutdown(context, request, response);
}

::grpc::Status CGrpcSyncService::Status(::grpc::ServerContext* context,
                                        const odc::StatusRequest* request,
                                        odc::StatusReply* response)
{
    return m_service->Status(context, request, response);
}
