// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __DDSControl__DDSControlClient__
#define __DDSControl__DDSControlClient__

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <grpcpp/grpcpp.h>

#include "odc.grpc.pb.h"

class DDSControlClient
{
  public:
    DDSControlClient(std::shared_ptr<grpc::Channel> channel);

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
    std::unique_ptr<ddscontrol::DDSControl::Stub> m_stub;
    std::string m_topo;
};

#endif /* defined(__DDSControl__DDSControlClient__) */
