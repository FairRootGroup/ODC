// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "DDSControlServer.h"
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace ddscontrol;

void DDSControlServer::Run(const std::string& _host, const DDSControlService::SConfigParams& _params)
{
    DDSControlService service(_params);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    // TODO: no shutdown implemented
    server->Wait();
}
