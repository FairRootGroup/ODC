/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__GrpcService__
#define __ODC__GrpcService__

// ODC
#include <odc/ControlService.h>
#include <odc/DDSSubmit.h>
// GRPC
#include <odc/grpc/odc.grpc.pb.h>
#include <grpcpp/grpcpp.h>

namespace odc::grpc {

class CGrpcService final
{
  public:
    CGrpcService() {}

    void setTimeout(const std::chrono::seconds& timeout);
    void registerResourcePlugins(const odc::core::CPluginManager::PluginMap_t& pluginMap);
    void registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& triggerMap);
    void restore(const std::string& restoreId);

    ::grpc::Status Initialize(::grpc::ServerContext*    ctx, const odc::InitializeRequest*    req, odc::GeneralReply* rep);
    ::grpc::Status Submit(::grpc::ServerContext*        ctx, const odc::SubmitRequest*        req, odc::GeneralReply* rep);
    ::grpc::Status Activate(::grpc::ServerContext*      ctx, const odc::ActivateRequest*      req, odc::GeneralReply* rep);
    ::grpc::Status Run(::grpc::ServerContext*           ctx, const odc::RunRequest*           req, odc::GeneralReply* rep);
    ::grpc::Status GetState(::grpc::ServerContext*      ctx, const odc::StateRequest*         req, odc::StateReply*   rep);
    ::grpc::Status SetProperties(::grpc::ServerContext* ctx, const odc::SetPropertiesRequest* req, odc::GeneralReply* rep);
    ::grpc::Status Update(::grpc::ServerContext*        ctx, const odc::UpdateRequest*        req, odc::GeneralReply* rep);
    ::grpc::Status Configure(::grpc::ServerContext*     ctx, const odc::ConfigureRequest*     req, odc::StateReply*   rep);
    ::grpc::Status Start(::grpc::ServerContext*         ctx, const odc::StartRequest*         req, odc::StateReply*   rep);
    ::grpc::Status Stop(::grpc::ServerContext*          ctx, const odc::StopRequest*          req, odc::StateReply*   rep);
    ::grpc::Status Reset(::grpc::ServerContext*         ctx, const odc::ResetRequest*         req, odc::StateReply*   rep);
    ::grpc::Status Terminate(::grpc::ServerContext*     ctx, const odc::TerminateRequest*     req, odc::StateReply*   rep);
    ::grpc::Status Shutdown(::grpc::ServerContext*      ctx, const odc::ShutdownRequest*      req, odc::GeneralReply* rep);
    ::grpc::Status Status(::grpc::ServerContext*        ctx, const odc::StatusRequest*        req, odc::StatusReply*  rep);

  private:
    odc::Error* newError(const odc::core::BaseRequestResult& res);
    void setupGeneralReply(odc::GeneralReply* rep, const odc::core::RequestResult& res);
    void setupStateReply(odc::StateReply* rep, const odc::core::RequestResult& res);
    void setupStatusReply(odc::StatusReply* rep, const odc::core::StatusRequestResult& res);
    std::mutex& getMutex(const std::string& partitionID);

    template<typename Response_t>
    void logResponse(const std::string& msg, const core::CommonParams& common, const Response_t* res);
    void logResponse(const std::string& msg, const core::CommonParams& common, const odc::StateReply* rep);

    odc::core::ControlService mService; ///< Core ODC service

    // Mutex for each partition.
    // All requests for a certain partition are processed sequentially.
    std::map<std::string, std::mutex> mMutexMap; ///< Mutex for each partition
    std::mutex mMutexMapMutex;                   ///< Mutex of global mutex map
};

} // namespace odc::grpc

#endif /* defined(__ODC__GrpcService__) */
