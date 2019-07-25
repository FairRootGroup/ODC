// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "DDSControlServer.h"
#include "DDSControlService.h"
// GRPC
#include "ddscontrol.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace ddscontrol;

void DDSControlServer::Run(const std::string& _host)
{
    DDSControlService service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    // TODO: no shutdown implemented
    server->Wait();
}
