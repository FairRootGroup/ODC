// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcControlServer.h"
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace odc::grpc;

void CGrpcControlServer::Run(const std::string& _host, const std::string& _rmsPlugin, const std::string& _configFile)
{
    CGrpcControlService service(_rmsPlugin, _configFile);

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, ::grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());

    // TODO: no shutdown implemented
    server->Wait();
}
