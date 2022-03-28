/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_GRPCCONTROLCLIENT
#define ODC_GRPCCONTROLCLIENT

// ODC
#include <odc/CliServiceHelper.h>
// STD
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
// GRPC
#include <grpcpp/grpcpp.h>
#include <odc/grpc/odc.grpc.pb.h>

class GrpcControlClient : public odc::core::CliServiceHelper<GrpcControlClient>
{
  public:
    GrpcControlClient(std::shared_ptr<grpc::Channel> channel)
        : mStub(odc::ODC::NewStub(channel))
    {}

    std::string requestInitialize(const odc::core::CommonParams& common, const odc::core::InitializeParams& initParams)
    {
        odc::InitializeRequest request;
        updateCommonParams(common, &request);
        request.set_sessionid(initParams.mDDSSessionID);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Initialize(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    std::string requestSubmit(const odc::core::CommonParams& common, const odc::core::SubmitParams& submitParams)
    {
        odc::SubmitRequest request;
        updateCommonParams(common, &request);
        request.set_plugin(submitParams.m_plugin);
        request.set_resources(submitParams.m_resources);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Submit(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    std::string requestActivate(const odc::core::CommonParams& common, const odc::core::ActivateParams& activateParams)
    {
        odc::ActivateRequest request;
        updateCommonParams(common, &request);
        request.set_topology(activateParams.mDDSTopologyFile);
        request.set_content(activateParams.mDDSTopologyContent);
        request.set_script(activateParams.mDDSTopologyScript);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Activate(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    std::string requestRun(const odc::core::CommonParams& common,
                           const odc::core::InitializeParams& /* initializeParams */,
                           const odc::core::SubmitParams& submitParams,
                           const odc::core::ActivateParams& activateParams)
    {
        odc::RunRequest request;
        updateCommonParams(common, &request);
        request.set_plugin(submitParams.m_plugin);
        request.set_resources(submitParams.m_resources);
        request.set_topology(activateParams.mDDSTopologyFile);
        request.set_content(activateParams.mDDSTopologyContent);
        request.set_script(activateParams.mDDSTopologyScript);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Run(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    std::string requestUpscale(const odc::core::CommonParams& common, const odc::core::UpdateParams& updateParams) { return updateRequest(common, updateParams); }
    std::string requestDownscale(const odc::core::CommonParams& common, const odc::core::UpdateParams& updateParams) { return updateRequest(common, updateParams); }

    std::string requestGetState(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams)
    {
        odc::StateRequest request;
        updateCommonParams(common, &request);
        request.set_path(deviceParams.m_path);
        request.set_detailed(deviceParams.m_detailed);
        odc::StateReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->GetState(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    std::string requestSetProperties(const odc::core::CommonParams& common, const odc::core::SetPropertiesParams& setPropsParams)
    {
        odc::SetPropertiesRequest request;
        updateCommonParams(common, &request);
        request.set_path(setPropsParams.m_path);
        for (const auto& v : setPropsParams.m_properties) {
            auto prop = request.add_properties();
            prop->set_key(v.first);
            prop->set_value(v.second);
        }
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->SetProperties(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    std::string requestConfigure(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams)
    {
        return stateChangeRequest<odc::ConfigureRequest>(common, deviceParams, &odc::ODC::Stub::Configure);
    }

    std::string requestStart(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams)
    {
        return stateChangeRequest<odc::StartRequest>(common, deviceParams, &odc::ODC::Stub::Start);
    }

    std::string requestStop(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams)
    {
        return stateChangeRequest<odc::StopRequest>(common, deviceParams, &odc::ODC::Stub::Stop);
    }

    std::string requestReset(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams)
    {
        return stateChangeRequest<odc::ResetRequest>(common, deviceParams, &odc::ODC::Stub::Reset);
    }

    std::string requestTerminate(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams)
    {
        return stateChangeRequest<odc::TerminateRequest>(common, deviceParams, &odc::ODC::Stub::Terminate);
    }

    std::string requestShutdown(const odc::core::CommonParams& common)
    {
        odc::ShutdownRequest request;
        updateCommonParams(common, &request);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Shutdown(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    std::string requestStatus(const odc::core::StatusParams& statusParams)
    {
        odc::StatusRequest request;
        request.set_running(statusParams.m_running);
        odc::StatusReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Status(&context, request, &reply);
        return GetReplyString(status, reply);
    }

  private:
    std::string updateRequest(const odc::core::CommonParams& common, const odc::core::UpdateParams& updateParams)
    {
        odc::UpdateRequest request;
        updateCommonParams(common, &request);
        request.set_topology(updateParams.mDDSTopologyFile);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Update(&context, request, &reply);
        return GetReplyString(status, reply);
    }

    template<typename Request, typename StubFunc>
    std::string stateChangeRequest(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams, StubFunc stubFunc)
    {
        // Protobuf message takes the ownership and deletes the object
        odc::StateRequest* stateChange = new odc::StateRequest();
        updateCommonParams(common, stateChange);
        stateChange->set_path(deviceParams.m_path);
        stateChange->set_detailed(deviceParams.m_detailed);

        Request request;
        request.set_allocated_request(stateChange);

        odc::StateReply reply;
        grpc::ClientContext context;
        grpc::Status status = (mStub.get()->*stubFunc)(&context, request, &reply);
        return GetReplyString(status, reply);
    }

  private:
    template<typename Reply>
    std::string GetReplyString(const grpc::Status& status, const Reply& rep)
    {
        if (status.ok()) {
            return rep.DebugString();
        } else {
            std::stringstream ss;
            ss << "RPC failed with error code " << status.error_code() << ": " << status.error_message() << std::endl;
            return ss.str();
        }
    }

    template<typename Request>
    void updateCommonParams(const odc::core::CommonParams& common, Request* req)
    {
        req->set_partitionid(common.mPartitionID);
        req->set_runnr(common.mRunNr);
        req->set_timeout(common.mTimeout);
    }

  private:
    std::unique_ptr<odc::ODC::Stub> mStub;
};

#endif /* defined(ODC_GRPCCONTROLCLIENT) */
