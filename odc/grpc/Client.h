/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_GRPCCLIENT
#define ODC_GRPCCLIENT

#include <odc/CliControllerHelper.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <grpcpp/grpcpp.h>
#include <odc/grpc/odc.grpc.pb.h>

class GrpcClient : public odc::core::CliControllerHelper<GrpcClient>
{
  public:
    GrpcClient(std::shared_ptr<grpc::Channel> channel)
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
        return GetGeneralReplyString(status, reply);
    }

    std::string requestSubmit(const odc::core::CommonParams& common, const odc::core::SubmitParams& submitParams)
    {
        odc::SubmitRequest request;
        updateCommonParams(common, &request);
        request.set_plugin(submitParams.mPlugin);
        request.set_resources(submitParams.mResources);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Submit(&context, request, &reply);
        return GetGeneralReplyString(status, reply);
    }

    std::string requestActivate(const odc::core::CommonParams& common, const odc::core::ActivateParams& activateParams)
    {
        odc::ActivateRequest request;
        updateCommonParams(common, &request);
        request.set_topology(activateParams.mTopoFile);
        request.set_content(activateParams.mTopoContent);
        request.set_script(activateParams.mTopoScript);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Activate(&context, request, &reply);
        return GetGeneralReplyString(status, reply);
    }

    std::string requestRun(const odc::core::CommonParams& common, const odc::core::RunParams& runParams)
    {
        odc::RunRequest request;
        updateCommonParams(common, &request);
        request.set_plugin(runParams.mPlugin);
        request.set_resources(runParams.mResources);
        request.set_topology(runParams.mTopoFile);
        request.set_content(runParams.mTopoContent);
        request.set_script(runParams.mTopoScript);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Run(&context, request, &reply);
        return GetGeneralReplyString(status, reply);
    }

    std::string requestUpscale(const odc::core::CommonParams& common, const odc::core::UpdateParams& updateParams) { return updateRequest(common, updateParams); }
    std::string requestDownscale(const odc::core::CommonParams& common, const odc::core::UpdateParams& updateParams) { return updateRequest(common, updateParams); }

    std::string requestGetState(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams)
    {
        odc::StateRequest request;
        updateCommonParams(common, &request);
        request.set_path(deviceParams.mPath);
        request.set_detailed(deviceParams.mDetailed);
        odc::StateReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->GetState(&context, request, &reply);
        return GetStateReplyString(status, reply);
    }

    std::string requestSetProperties(const odc::core::CommonParams& common, const odc::core::SetPropertiesParams& setPropsParams)
    {
        odc::SetPropertiesRequest request;
        updateCommonParams(common, &request);
        request.set_path(setPropsParams.mPath);
        for (const auto& v : setPropsParams.mProperties) {
            auto prop = request.add_properties();
            prop->set_key(v.first);
            prop->set_value(v.second);
        }
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->SetProperties(&context, request, &reply);
        return GetGeneralReplyString(status, reply);
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
        return GetGeneralReplyString(status, reply);
    }

    std::string requestStatus(const odc::core::StatusParams& statusParams)
    {
        odc::StatusRequest request;
        request.set_running(statusParams.mRunning);
        odc::StatusReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Status(&context, request, &reply);
        return GetStatusReplyString(status, reply);
    }

  private:
    std::string updateRequest(const odc::core::CommonParams& common, const odc::core::UpdateParams& updateParams)
    {
        odc::UpdateRequest request;
        updateCommonParams(common, &request);
        request.set_topology(updateParams.mTopoFile);
        odc::GeneralReply reply;
        grpc::ClientContext context;
        grpc::Status status = mStub->Update(&context, request, &reply);
        return GetGeneralReplyString(status, reply);
    }

    template<typename Request, typename StubFunc>
    std::string stateChangeRequest(const odc::core::CommonParams& common, const odc::core::DeviceParams& deviceParams, StubFunc stubFunc)
    {
        // Protobuf message takes the ownership and deletes the object
        odc::StateRequest* stateChange = new odc::StateRequest();
        updateCommonParams(common, stateChange);
        stateChange->set_path(deviceParams.mPath);
        stateChange->set_detailed(deviceParams.mDetailed);

        Request request;
        request.set_allocated_request(stateChange);

        odc::StateReply reply;
        grpc::ClientContext context;
        grpc::Status status = (mStub.get()->*stubFunc)(&context, request, &reply);
        return GetStateReplyString(status, reply);
    }

  private:
    std::string GetGeneralReplyString(const grpc::Status& status, const odc::GeneralReply& rep)
    {
        std::stringstream ss;
        if (status.ok()) {
            ss << "  msg: "            << rep.msg()
               << ", Partition ID: "   << rep.partitionid()
               << ", Run Nr.: "        << rep.runnr()
               << ", DDS Session ID: " << rep.sessionid()
               << ", topology state: " << rep.state()
               << ", execution time: " << rep.exectime() << "ms";
            if (!rep.hosts().empty()) {
                ss << "\n  Hosts:\n    ";
                for (int i = 0; i < rep.hosts().size(); ++i) {
                    ss << rep.hosts().at(i) << (i == (rep.hosts().size() - 1) ? "" : ", ");
                }
            }

            if (rep.status() == odc::ReplyStatus::ERROR) {
                ss << ", ERROR: " << rep.error().msg() << " (" << rep.error().code() << ")\n";
                return ss.str();
            } else if (rep.status() == odc::ReplyStatus::SUCCESS) {
                ss << "\n";
                return ss.str();
            } else {
                return rep.DebugString();
            }
        } else {
            ss << "  RPC failed with error code " << status.error_code() << ": " << status.error_message() << "\n";
            return ss.str();
        }
    }

    std::string GetStateReplyString(const grpc::Status& status, const odc::StateReply& rep)
    {
        std::stringstream ss;
        if (status.ok()) {
            ss << GetGeneralReplyString(status, rep.reply());
            if (!rep.devices().empty()) {
                ss << "  Devices:\n";
                for (const auto& d : rep.devices()) {
                    ss << "    id: " << d.id()
                    << ", state: "   << d.state()
                    << ", ignored: " << d.ignored()
                    << ", host: "    << d.host()
                    << ", path: "    << d.path() << "\n";
                }
            }
            return ss.str();
        } else {
            ss << "  RPC failed with error code " << status.error_code() << ": " << status.error_message() << std::endl;
            return ss.str();
        }
    }

    std::string GetStatusReplyString(const grpc::Status& status, const odc::StatusReply& rep)
    {
        std::stringstream ss;
        if (status.ok()) {
            if (rep.status() == odc::ReplyStatus::SUCCESS) {
                ss << "  msg: " << rep.msg() << "\n";
                ss << "  found " << rep.partitions().size() << " partition(s)" << (rep.partitions().size() > 0 ? ":" : "") << "\n";
                for (const auto& p : rep.partitions()) {
                    ss << "    Partition ID: " << p.partitionid()
                       << ", DDS session: " << odc::SessionStatus_Name(p.status())
                       << ", DDS session ID: " << p.sessionid()
                       << ", Run Nr.: " << p.runnr()
                       << ", topology state: " << p.state() << "\n";
                }
                ss << "  execution time: " << rep.exectime() << "ms\n";
            } else {
                ss << "Status: " << rep.DebugString();
            }
            return ss.str();
        } else {
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

#endif /* defined(ODC_GRPCCLIENT) */
