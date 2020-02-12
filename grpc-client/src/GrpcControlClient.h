// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlClient__
#define __ODC__GrpcControlClient__

// ODC
#include "CliServiceHelper.h"
// STD
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class CGrpcControlClient : public odc::core::CCliServiceHelper<CGrpcControlClient>
{
  public:
    CGrpcControlClient(std::shared_ptr<grpc::Channel> channel);

    std::string requestInitialize(const odc::core::SInitializeParams& _params);
    std::string requestSubmit(const odc::core::SSubmitParams& _params);
    std::string requestActivate(const odc::core::SActivateParams& _params);
    std::string requestUpscale(const odc::core::SUpdateParams& _params);
    std::string requestDownscale(const odc::core::SUpdateParams& _params);
    std::string requestConfigure();
    std::string requestStart();
    std::string requestStop();
    std::string requestReset();
    std::string requestTerminate();
    std::string requestShutdown();

  private:
    std::string updateRequest(const odc::core::SUpdateParams& _params);

  private:
    template <typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply);

  private:
    std::unique_ptr<odc::ODC::Stub> m_stub;
};

#endif /* defined(__ODC__GrpcControlClient__) */
