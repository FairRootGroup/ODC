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

    void setTimeout(const std::chrono::seconds& _timeout);
    void registerResourcePlugins(const odc::core::CPluginManager::PluginMap_t& _pluginMap);
    void registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& _triggerMap);
    void restore(const std::string& _restoreId);

    ::grpc::Status Initialize(::grpc::ServerContext*    context, const odc::InitializeRequest*    request, odc::GeneralReply* response);
    ::grpc::Status Submit(::grpc::ServerContext*        context, const odc::SubmitRequest*        request, odc::GeneralReply* response);
    ::grpc::Status Activate(::grpc::ServerContext*      context, const odc::ActivateRequest*      request, odc::GeneralReply* response);
    ::grpc::Status Run(::grpc::ServerContext*           context, const odc::RunRequest*           request, odc::GeneralReply* response);
    ::grpc::Status GetState(::grpc::ServerContext*      context, const odc::StateRequest*         request, odc::StateReply*   response);
    ::grpc::Status SetProperties(::grpc::ServerContext* context, const odc::SetPropertiesRequest* request, odc::GeneralReply* response);
    ::grpc::Status Update(::grpc::ServerContext*        context, const odc::UpdateRequest*        request, odc::GeneralReply* response);
    ::grpc::Status Configure(::grpc::ServerContext*     context, const odc::ConfigureRequest*     request, odc::StateReply*   response);
    ::grpc::Status Start(::grpc::ServerContext*         context, const odc::StartRequest*         request, odc::StateReply*   response);
    ::grpc::Status Stop(::grpc::ServerContext*          context, const odc::StopRequest*          request, odc::StateReply*   response);
    ::grpc::Status Reset(::grpc::ServerContext*         context, const odc::ResetRequest*         request, odc::StateReply*   response);
    ::grpc::Status Terminate(::grpc::ServerContext*     context, const odc::TerminateRequest*     request, odc::StateReply*   response);
    ::grpc::Status Shutdown(::grpc::ServerContext*      context, const odc::ShutdownRequest*      request, odc::GeneralReply* response);
    ::grpc::Status Status(::grpc::ServerContext*        context, const odc::StatusRequest*        request, odc::StatusReply*  response);

  private:
    odc::Error* newError(const odc::core::BaseRequestResult& result);
    void setupGeneralReply(odc::GeneralReply* _response, const odc::core::RequestResult& result);
    void setupStateReply(odc::StateReply* _response, const odc::core::RequestResult& result);
    void setupStatusReply(odc::StatusReply* _response, const odc::core::StatusRequestResult& result);
    std::mutex& getMutex(const std::string& _partitionID);

    template<typename Request_t>
    core::SCommonParams commonParams(const Request_t* _request);

    template<typename Response_t>
    void logResponse(const std::string& _msg, const core::SCommonParams& _common, const Response_t* _response);
    void logResponse(const std::string& _msg, const core::SCommonParams& _common, const odc::StateReply* _response);

    odc::core::ControlService m_service; ///< Core ODC service

    // Mutex for each partition.
    // All requests for a certain partition are processed sequentially.
    std::map<std::string, std::mutex> m_mutexMap; ///< Mutex for each partition
    std::mutex m_mutexMapMutex;                   ///< Mutex of global mutex map
};

} // namespace odc::grpc

#endif /* defined(__ODC__GrpcService__) */
