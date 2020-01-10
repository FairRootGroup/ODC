// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcControlServer.h"
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace odc::grpc;

void GrpcControlServer::Run(const std::string& _host, const odc::core::ControlService::SConfigParams& _params)
{
    GrpcControlService service(_params);

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, ::grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());

    // TODO: no shutdown implemented
    server->Wait();
}
