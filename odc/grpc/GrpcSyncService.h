/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__GrpcSyncService__
#define __ODC__GrpcSyncService__

// ODC
#include <odc/grpc/GrpcService.h>
#include <odc/PluginManager.h>
// GRPC
#include <odc/grpc/odc.grpc.pb.h>
#include <grpcpp/grpcpp.h>

namespace odc::grpc {

class CGrpcSyncService final : public odc::ODC::Service
{
  public:
    CGrpcSyncService() {}

    void run(const std::string& host)
    {
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(host, ::grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
        server->Wait();
    }
    void setTimeout(const std::chrono::seconds& timeout) { mService.setTimeout(timeout); }
    void registerResourcePlugins(const odc::core::CPluginManager::PluginMap_t& pluginMap) { mService.registerResourcePlugins(pluginMap); }
    void registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& triggerMap) { mService.registerRequestTriggers(triggerMap); }
    void restore(const std::string& restoreId) { mService.restore(restoreId); }

  private:
    ::grpc::Status Initialize(::grpc::ServerContext*    ctx, const odc::InitializeRequest*    req, odc::GeneralReply* rep) override { return mService.Initialize(ctx, req, rep); }
    ::grpc::Status Submit(::grpc::ServerContext*        ctx, const odc::SubmitRequest*        req, odc::GeneralReply* rep) override { return mService.Submit(ctx, req, rep); }
    ::grpc::Status Activate(::grpc::ServerContext*      ctx, const odc::ActivateRequest*      req, odc::GeneralReply* rep) override { return mService.Activate(ctx, req, rep); }
    ::grpc::Status Run(::grpc::ServerContext*           ctx, const odc::RunRequest*           req, odc::GeneralReply* rep) override { return mService.Run(ctx, req, rep); }
    ::grpc::Status Update(::grpc::ServerContext*        ctx, const odc::UpdateRequest*        req, odc::GeneralReply* rep) override { return mService.Update(ctx, req, rep); }
    ::grpc::Status GetState(::grpc::ServerContext*      ctx, const odc::StateRequest*         req, odc::StateReply*   rep) override { return mService.GetState(ctx, req, rep); }
    ::grpc::Status SetProperties(::grpc::ServerContext* ctx, const odc::SetPropertiesRequest* req, odc::GeneralReply* rep) override { return mService.SetProperties(ctx, req, rep); }
    ::grpc::Status Configure(::grpc::ServerContext*     ctx, const odc::ConfigureRequest*     req, odc::StateReply*   rep) override { return mService.Configure(ctx, req, rep); }
    ::grpc::Status Start(::grpc::ServerContext*         ctx, const odc::StartRequest*         req, odc::StateReply*   rep) override { return mService.Start(ctx, req, rep); }
    ::grpc::Status Stop(::grpc::ServerContext*          ctx, const odc::StopRequest*          req, odc::StateReply*   rep) override { return mService.Stop(ctx, req, rep); }
    ::grpc::Status Reset(::grpc::ServerContext*         ctx, const odc::ResetRequest*         req, odc::StateReply*   rep) override { return mService.Reset(ctx, req, rep); }
    ::grpc::Status Terminate(::grpc::ServerContext*     ctx, const odc::TerminateRequest*     req, odc::StateReply*   rep) override { return mService.Terminate(ctx, req, rep); }
    ::grpc::Status Shutdown(::grpc::ServerContext*      ctx, const odc::ShutdownRequest*      req, odc::GeneralReply* rep) override { return mService.Shutdown(ctx, req, rep); }
    ::grpc::Status Status(::grpc::ServerContext*        ctx, const odc::StatusRequest*        req, odc::StatusReply*  rep) override { return mService.Status(ctx, req, rep); }

    odc::grpc::CGrpcService mService; ///< Core gRPC service
};

} // namespace odc::grpc

#endif /* defined(__ODC__GrpcSyncService__) */
