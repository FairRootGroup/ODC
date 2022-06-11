/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_GRPC_SYNCCONTROLLER
#define ODC_GRPC_SYNCCONTROLLER

// ODC
#include <odc/grpc/Controller.h>
#include <odc/PluginManager.h>
// GRPC
#include <odc/grpc/odc.grpc.pb.h>
#include <grpcpp/grpcpp.h>

namespace odc::grpc {

class SyncController final : public odc::ODC::Service
{
  public:
    SyncController() {}

    void run(const std::string& host)
    {
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(host, ::grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
        server->Wait();
    }
    void setTimeout(const std::chrono::seconds& timeout) { mCtrl.setTimeout(timeout); }
    void setHistoryDir(const std::string& dir) { mCtrl.setHistoryDir(dir); }
    void registerResourcePlugins(const odc::core::PluginManager::PluginMap_t& pluginMap) { mCtrl.registerResourcePlugins(pluginMap); }
    void registerRequestTriggers(const odc::core::PluginManager::PluginMap_t& triggerMap) { mCtrl.registerRequestTriggers(triggerMap); }
    void restore(const std::string& restoreId, const std::string& restoreDir) { mCtrl.restore(restoreId, restoreDir); }

  private:
    ::grpc::Status Initialize(::grpc::ServerContext*    ctx, const odc::InitializeRequest*    req, odc::GeneralReply* rep) override { return mCtrl.Initialize(ctx, req, rep); }
    ::grpc::Status Submit(::grpc::ServerContext*        ctx, const odc::SubmitRequest*        req, odc::GeneralReply* rep) override { return mCtrl.Submit(ctx, req, rep); }
    ::grpc::Status Activate(::grpc::ServerContext*      ctx, const odc::ActivateRequest*      req, odc::GeneralReply* rep) override { return mCtrl.Activate(ctx, req, rep); }
    ::grpc::Status Run(::grpc::ServerContext*           ctx, const odc::RunRequest*           req, odc::GeneralReply* rep) override { return mCtrl.Run(ctx, req, rep); }
    ::grpc::Status Update(::grpc::ServerContext*        ctx, const odc::UpdateRequest*        req, odc::GeneralReply* rep) override { return mCtrl.Update(ctx, req, rep); }
    ::grpc::Status GetState(::grpc::ServerContext*      ctx, const odc::StateRequest*         req, odc::StateReply*   rep) override { return mCtrl.GetState(ctx, req, rep); }
    ::grpc::Status SetProperties(::grpc::ServerContext* ctx, const odc::SetPropertiesRequest* req, odc::GeneralReply* rep) override { return mCtrl.SetProperties(ctx, req, rep); }
    ::grpc::Status Configure(::grpc::ServerContext*     ctx, const odc::ConfigureRequest*     req, odc::StateReply*   rep) override { return mCtrl.Configure(ctx, req, rep); }
    ::grpc::Status Start(::grpc::ServerContext*         ctx, const odc::StartRequest*         req, odc::StateReply*   rep) override { return mCtrl.Start(ctx, req, rep); }
    ::grpc::Status Stop(::grpc::ServerContext*          ctx, const odc::StopRequest*          req, odc::StateReply*   rep) override { return mCtrl.Stop(ctx, req, rep); }
    ::grpc::Status Reset(::grpc::ServerContext*         ctx, const odc::ResetRequest*         req, odc::StateReply*   rep) override { return mCtrl.Reset(ctx, req, rep); }
    ::grpc::Status Terminate(::grpc::ServerContext*     ctx, const odc::TerminateRequest*     req, odc::StateReply*   rep) override { return mCtrl.Terminate(ctx, req, rep); }
    ::grpc::Status Shutdown(::grpc::ServerContext*      ctx, const odc::ShutdownRequest*      req, odc::GeneralReply* rep) override { return mCtrl.Shutdown(ctx, req, rep); }
    ::grpc::Status Status(::grpc::ServerContext*        ctx, const odc::StatusRequest*        req, odc::StatusReply*  rep) override { return mCtrl.Status(ctx, req, rep); }

    odc::grpc::Controller mCtrl; ///< Core gRPC service
};

} // namespace odc::grpc

#endif /* defined(ODC_GRPC_SYNCCONTROLLER) */
