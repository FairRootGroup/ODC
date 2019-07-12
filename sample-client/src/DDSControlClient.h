// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <grpcpp/grpcpp.h>

#include "ddscontrol.grpc.pb.h"

class DDSControlClient
{
  public:
    DDSControlClient(std::shared_ptr<grpc::Channel> channel);

    std::string RequestInitialize();
    std::string RequestConfigureRun();
    std::string RequestStart();
    std::string RequestStop();
    std::string RequestTerminate();

  private:
    template <typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply);

  private:
    std::unique_ptr<ddscontrol::DDSControl::Stub> m_stub;
};
