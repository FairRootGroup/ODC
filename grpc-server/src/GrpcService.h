// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcService__
#define __ODC__GrpcService__

// ODC
#include "ControlService.h"
#include "DDSSubmit.h"
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace odc::grpc
{
    class CGrpcService final
    {
      public:
        CGrpcService();

        void setTimeout(const std::chrono::seconds& _timeout);
        void registerResourcePlugins(const odc::core::CPluginManager::PluginMap_t& _pluginMap);
        void registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& _triggerMap);

        ::grpc::Status Initialize(::grpc::ServerContext* context,
                                  const odc::InitializeRequest* request,
                                  odc::GeneralReply* response);
        ::grpc::Status Submit(::grpc::ServerContext* context,
                              const odc::SubmitRequest* request,
                              odc::GeneralReply* response);
        ::grpc::Status Activate(::grpc::ServerContext* context,
                                const odc::ActivateRequest* request,
                                odc::GeneralReply* response);
        ::grpc::Status Run(::grpc::ServerContext* context, const odc::RunRequest* request, odc::GeneralReply* response);
        ::grpc::Status GetState(::grpc::ServerContext* context,
                                const odc::StateRequest* request,
                                odc::StateReply* response);
        ::grpc::Status SetProperties(::grpc::ServerContext* context,
                                     const odc::SetPropertiesRequest* request,
                                     odc::GeneralReply* response);
        ::grpc::Status Update(::grpc::ServerContext* context,
                              const odc::UpdateRequest* request,
                              odc::GeneralReply* response);
        ::grpc::Status Configure(::grpc::ServerContext* context,
                                 const odc::ConfigureRequest* request,
                                 odc::StateReply* response);
        ::grpc::Status Start(::grpc::ServerContext* context,
                             const odc::StartRequest* request,
                             odc::StateReply* response);
        ::grpc::Status Stop(::grpc::ServerContext* context, const odc::StopRequest* request, odc::StateReply* response);
        ::grpc::Status Reset(::grpc::ServerContext* context,
                             const odc::ResetRequest* request,
                             odc::StateReply* response);
        ::grpc::Status Terminate(::grpc::ServerContext* context,
                                 const odc::TerminateRequest* request,
                                 odc::StateReply* response);
        ::grpc::Status Shutdown(::grpc::ServerContext* context,
                                const odc::ShutdownRequest* request,
                                odc::GeneralReply* response);
        ::grpc::Status Status(::grpc::ServerContext* context,
                              const odc::StatusRequest* request,
                              odc::StatusReply* response);

      private:
        odc::Error* newError(const odc::core::SBaseReturnValue& _value);
        void setupGeneralReply(odc::GeneralReply* _response, const odc::core::SReturnValue& _value);
        void setupStateReply(odc::StateReply* _response, const odc::core::SReturnValue& _value);
        void setupStatusReply(odc::StatusReply* _response, const odc::core::SStatusReturnValue& _value);
        std::mutex& getMutex(const odc::core::partitionID_t& _partitionID);

        std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service

        // Mutex for each partition.
        // All requests for a certain partition are processed sequentially.
        std::map<std::string, std::mutex> m_mutexMap; ///< Mutex for each partition
        std::mutex m_mutexMapMutex;                   ///< Mutex of global mutex map
    };
} // namespace odc::grpc

#endif /* defined(__ODC__GrpcService__) */
