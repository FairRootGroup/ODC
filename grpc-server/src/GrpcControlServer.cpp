// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcControlServer.h"
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using namespace odc::grpc;

void CGrpcControlServer::Run(const std::string& _host)
{
    CGrpcControlService service;
    service.setSubmitParams(m_submitParams);
    service.setQCTopoPath(m_qcTopoPath);
    service.setRecoTopoPath(m_recoTopoPath);

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, ::grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());

    // TODO: no shutdown implemented
    server->Wait();
}

void CGrpcControlServer::setSubmitParams(const odc::core::SSubmitParams& _params)
{
    m_submitParams = _params;
}

void CGrpcControlServer::setRecoTopoPath(const std::string& _path)
{
    m_recoTopoPath = _path;
}

void CGrpcControlServer::setQCTopoPath(const std::string& _path)
{
    m_qcTopoPath = _path;
}
