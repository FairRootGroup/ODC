// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlClient__
#define __ODC__GrpcControlClient__

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <grpcpp/grpcpp.h>

#include "odc.grpc.pb.h"

class GrpcControlClient
{
  public:
    GrpcControlClient(std::shared_ptr<grpc::Channel> channel);

    std::string RequestInitialize();
    std::string RequestConfigureRun();
    std::string RequestStart();
    std::string RequestStop();
    std::string RequestTerminate();
    std::string RequestShutdown();

  public:
    void setTopo(const std::string& _topo);

  private:
    template <typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply);

  private:
    std::unique_ptr<odc::ODC::Stub> m_stub;
    std::string m_topo;
};

#endif /* defined(__ODC__GrpcControlClient__) */
