// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcControlServer.h"
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace odc::grpc;
using namespace odc::core;
using namespace std;

CGrpcControlServer::CGrpcControlServer()
    : m_service(make_shared<CGrpcControlService>())
{
}

void CGrpcControlServer::Run(const std::string& _host)
{
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, ::grpc::InsecureServerCredentials());
    builder.RegisterService(m_service.get());
    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());

    // TODO: no shutdown implemented
    server->Wait();
}

void CGrpcControlServer::setTimeout(const std::chrono::seconds& _timeout)
{
    m_service->setTimeout(_timeout);
}

void CGrpcControlServer::registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap)
{
    m_service->registerResourcePlugins(_pluginMap);
}
