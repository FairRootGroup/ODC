// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcSyncService__
#define __ODC__GrpcSyncService__

// ODC
#include "GrpcService.h"
#include "PluginManager.h"
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace odc::grpc {

class CGrpcSyncService final : public odc::ODC::Service
{
  public:
    CGrpcSyncService() : m_service(std::make_shared<CGrpcService>()) {}

    void run(const std::string& _host)
    {
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(_host, ::grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
        server->Wait();
    }
    void setTimeout(const std::chrono::seconds& _timeout) { m_service->setTimeout(_timeout); }
    void registerResourcePlugins(const odc::core::CPluginManager::PluginMap_t& _pluginMap) { m_service->registerResourcePlugins(_pluginMap); }
    void registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& _triggerMap) { m_service->registerRequestTriggers(_triggerMap); }
    void restore(const std::string& _restoreId) { m_service->restore(_restoreId); }

  private:
    ::grpc::Status Initialize(::grpc::ServerContext*    context, const odc::InitializeRequest*    request, odc::GeneralReply* response) override { return m_service->Initialize(context, request, response); }
    ::grpc::Status Submit(::grpc::ServerContext*        context, const odc::SubmitRequest*        request, odc::GeneralReply* response) override { return m_service->Submit(context, request, response); }
    ::grpc::Status Activate(::grpc::ServerContext*      context, const odc::ActivateRequest*      request, odc::GeneralReply* response) override { return m_service->Activate(context, request, response); }
    ::grpc::Status Run(::grpc::ServerContext*           context, const odc::RunRequest*           request, odc::GeneralReply* response) override { return m_service->Run(context, request, response); }
    ::grpc::Status Update(::grpc::ServerContext*        context, const odc::UpdateRequest*        request, odc::GeneralReply* response) override { return m_service->Update(context, request, response); }
    ::grpc::Status GetState(::grpc::ServerContext*      context, const odc::StateRequest*         request, odc::StateReply*   response) override { return m_service->GetState(context, request, response); }
    ::grpc::Status SetProperties(::grpc::ServerContext* context, const odc::SetPropertiesRequest* request, odc::GeneralReply* response) override { return m_service->SetProperties(context, request, response); }
    ::grpc::Status Configure(::grpc::ServerContext*     context, const odc::ConfigureRequest*     request, odc::StateReply*   response) override { return m_service->Configure(context, request, response); }
    ::grpc::Status Start(::grpc::ServerContext*         context, const odc::StartRequest*         request, odc::StateReply*   response) override { return m_service->Start(context, request, response); }
    ::grpc::Status Stop(::grpc::ServerContext*          context, const odc::StopRequest*          request, odc::StateReply*   response) override { return m_service->Stop(context, request, response); }
    ::grpc::Status Reset(::grpc::ServerContext*         context, const odc::ResetRequest*         request, odc::StateReply*   response) override { return m_service->Reset(context, request, response); }
    ::grpc::Status Terminate(::grpc::ServerContext*     context, const odc::TerminateRequest*     request, odc::StateReply*   response) override { return m_service->Terminate(context, request, response); }
    ::grpc::Status Shutdown(::grpc::ServerContext*      context, const odc::ShutdownRequest*      request, odc::GeneralReply* response) override { return m_service->Shutdown(context, request, response); }
    ::grpc::Status Status(::grpc::ServerContext*        context, const odc::StatusRequest*        request, odc::StatusReply*  response) override { return m_service->Status(context, request, response); }

    std::shared_ptr<odc::grpc::CGrpcService> m_service; ///< Core gRPC service
};

} // namespace odc::grpc

#endif /* defined(__ODC__GrpcSyncService__) */
