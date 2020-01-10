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
            CGrpcControlService(const std::string& _rmsPlugin, const std::string& _configFile);

          private:
            ::grpc::Status Initialize(::grpc::ServerContext* context,
                                      const odc::InitializeRequest* request,
                                      odc::GeneralReply* response) override;
            ::grpc::Status ConfigureRun(::grpc::ServerContext* context,
                                        const odc::ConfigureRunRequest* request,
                                        odc::GeneralReply* response) override;
            ::grpc::Status Start(::grpc::ServerContext* context,
                                 const odc::StartRequest* request,
                                 odc::GeneralReply* response) override;
            ::grpc::Status Stop(::grpc::ServerContext* context,
                                const odc::StopRequest* request,
                                odc::GeneralReply* response) override;
            ::grpc::Status Terminate(::grpc::ServerContext* context,
                                     const odc::TerminateRequest* request,
                                     odc::GeneralReply* response) override;
            ::grpc::Status Shutdown(::grpc::ServerContext* context,
                                    const odc::ShutdownRequest* request,
                                    odc::GeneralReply* response) override;

          private:
            void setupGeneralReply(odc::GeneralReply* _response, const odc::core::SReturnValue& _value);

          private:
            std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
            std::string m_rmsPlugin;                               ///< RMS plugin used for DDS
            std::string m_configFile;                              ///< Configuration file for RMS plugin
        };
    } // namespace grpc
} // namespace odc

#endif /* defined(__ODC__GrpcControlService__) */
