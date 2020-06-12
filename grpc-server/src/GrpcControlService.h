// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlService__
#define __ODC__GrpcControlService__

// ODC
#include "ControlService.h"

// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace odc
{
    namespace grpc
    {
        class CGrpcControlService final : public odc::ODC::Service
        {
          public:
            CGrpcControlService();

            void setSubmitParams(const odc::core::SSubmitParams& _params);
            void setTimeout(const std::chrono::seconds& _timeout);

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
            ::grpc::Status Update(::grpc::ServerContext* context,
                                  const odc::UpdateRequest* request,
                                  odc::GeneralReply* response) override;
            ::grpc::Status Configure(::grpc::ServerContext* context,
                                     const odc::ConfigureRequest* request,
                                     odc::StateChangeReply* response) override;
            ::grpc::Status Start(::grpc::ServerContext* context,
                                 const odc::StartRequest* request,
                                 odc::StateChangeReply* response) override;
            ::grpc::Status Stop(::grpc::ServerContext* context,
                                const odc::StopRequest* request,
                                odc::StateChangeReply* response) override;
            ::grpc::Status Reset(::grpc::ServerContext* context,
                                 const odc::ResetRequest* request,
                                 odc::StateChangeReply* response) override;
            ::grpc::Status Terminate(::grpc::ServerContext* context,
                                     const odc::TerminateRequest* request,
                                     odc::StateChangeReply* response) override;
            ::grpc::Status Shutdown(::grpc::ServerContext* context,
                                    const odc::ShutdownRequest* request,
                                    odc::GeneralReply* response) override;

            void setupGeneralReply(odc::GeneralReply* _response, const odc::core::SReturnValue& _value);
            void setupStateChangeReply(odc::StateChangeReply* _response, const odc::core::SReturnValue& _value);

            std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
            odc::core::SSubmitParams m_submitParams;               ///< Parameters of the submit request
        };
    } // namespace grpc
} // namespace odc

#endif /* defined(__ODC__GrpcControlService__) */
