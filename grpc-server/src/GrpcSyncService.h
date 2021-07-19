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

namespace odc::grpc
{
    class CGrpcSyncService final : public odc::ODC::Service
    {
      public:
        CGrpcSyncService();

        void run(const std::string& _host);
        void setTimeout(const std::chrono::seconds& _timeout);
        void registerResourcePlugins(const odc::core::CPluginManager::PluginMap_t& _pluginMap);
        void registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& _triggerMap);

      private:
        ::grpc::Status Initialize(::grpc::ServerContext* context,
                                  const odc::InitializeRequest* request,
                                  odc::GeneralReply* response) override;
        ::grpc::Status Submit(::grpc::ServerContext* context,
                              const odc::SubmitRequest* request,
                              odc::GeneralReply* response) override;
        ::grpc::Status Activate(::grpc::ServerContext* context,
                                const odc::ActivateRequest* request,
                                odc::GeneralReply* response) override;
        ::grpc::Status Run(::grpc::ServerContext* context,
                           const odc::RunRequest* request,
                           odc::GeneralReply* response) override;
        ::grpc::Status GetState(::grpc::ServerContext* context,
                                const odc::StateRequest* request,
                                odc::StateReply* response) override;
        ::grpc::Status SetProperties(::grpc::ServerContext* context,
                                     const odc::SetPropertiesRequest* request,
                                     odc::GeneralReply* response) override;
        ::grpc::Status Update(::grpc::ServerContext* context,
                              const odc::UpdateRequest* request,
                              odc::GeneralReply* response) override;
        ::grpc::Status Configure(::grpc::ServerContext* context,
                                 const odc::ConfigureRequest* request,
                                 odc::StateReply* response) override;
        ::grpc::Status Start(::grpc::ServerContext* context,
                             const odc::StartRequest* request,
                             odc::StateReply* response) override;
        ::grpc::Status Stop(::grpc::ServerContext* context,
                            const odc::StopRequest* request,
                            odc::StateReply* response) override;
        ::grpc::Status Reset(::grpc::ServerContext* context,
                             const odc::ResetRequest* request,
                             odc::StateReply* response) override;
        ::grpc::Status Terminate(::grpc::ServerContext* context,
                                 const odc::TerminateRequest* request,
                                 odc::StateReply* response) override;
        ::grpc::Status Shutdown(::grpc::ServerContext* context,
                                const odc::ShutdownRequest* request,
                                odc::GeneralReply* response) override;
        ::grpc::Status Status(::grpc::ServerContext* context,
                              const odc::StatusRequest* request,
                              odc::StatusReply* response) override;

        std::shared_ptr<odc::grpc::CGrpcService> m_service; ///< Core gRPC service
    };
} // namespace odc::grpc

#endif /* defined(__ODC__GrpcSyncService__) */
