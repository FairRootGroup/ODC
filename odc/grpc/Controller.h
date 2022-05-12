/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_GRPC_CONTROLLER
#define ODC_GRPC_CONTROLLER

#include <odc/Controller.h>
#include <odc/DDSSubmit.h>
#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/Topology.h>

#include <grpcpp/grpcpp.h>
#include <odc/grpc/odc.grpc.pb.h>

#include <cassert>
#include <numeric>
#include <sstream>
#include <string>

namespace odc::grpc
{

class Controller final
{
  public:
    Controller() {}

    void setTimeout(const std::chrono::seconds& timeout) { mController.setTimeout(timeout); }
    void registerResourcePlugins(const core::PluginManager::PluginMap_t& pluginMap) { mController.registerResourcePlugins(pluginMap); }
    void registerRequestTriggers(const core::PluginManager::PluginMap_t& triggerMap) { mController.registerRequestTriggers(triggerMap); }
    void restore(const std::string& restoreId) { mController.restore(restoreId); }

    ::grpc::Status Initialize(::grpc::ServerContext* ctx, const odc::InitializeRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Initialize request from " << client << ":\n" << req->DebugString();
        core::InitializeParams initializeParams{ req->sessionid() };
        core::RequestResult res{ mController.execInitialize(common, initializeParams) };
        setupGeneralReply(rep, res);
        logResponse("Initialize response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Submit(::grpc::ServerContext* ctx, const odc::SubmitRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Submit request from " << client << ":\n" << req->DebugString();
        core::SubmitParams submitParams{ req->plugin(), req->resources() };
        core::RequestResult res{ mController.execSubmit(common, submitParams) };
        setupGeneralReply(rep, res);
        logResponse("Submit response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Activate(::grpc::ServerContext* ctx, const odc::ActivateRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Activate request from " << client << ":\n" << req->DebugString();
        core::ActivateParams activateParams{ req->topology(), req->content(), req->script() };
        core::RequestResult res{ mController.execActivate(common, activateParams) };
        setupGeneralReply(rep, res);
        logResponse("Activate response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Run(::grpc::ServerContext* ctx, const odc::RunRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Run request from " << client << ":\n" << req->DebugString();
        core::InitializeParams initializeParams{ "" };
        core::SubmitParams submitParams{ req->plugin(), req->resources() };
        core::ActivateParams activateParams{ req->topology(), req->content(), req->script() };
        core::RequestResult res{ mController.execRun(common, initializeParams, submitParams, activateParams) };
        setupGeneralReply(rep, res);
        logResponse("Run response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status GetState(::grpc::ServerContext* ctx, const odc::StateRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        OLOG(info, common) << "GetState request from " << client << ": "
                                 << "partitionId: " << req->partitionid()
                                 << ", runnr: "     << req->runnr()
                                 << ", timeout: "   << req->timeout()
                                 << ", path: "      << req->path()
                                 << ", detailed: "  << req->detailed();
        core::DeviceParams deviceParams{ req->path(), req->detailed() };
        core::RequestResult res{ mController.execGetState(common, deviceParams) };
        setupStateReply(rep, res);

        if (rep->reply().status() == odc::ReplyStatus::ERROR) {
            OLOG(error, common) << "GetState response: ERROR" << " (" << rep->reply().error().code() << ")"
                                      << ", sessionId: "   << rep->reply().sessionid()
                                      << ", partitionId: " << rep->reply().partitionid()
                                      << ", state: "       << rep->reply().state()
                                      << ", msg: "         << rep->reply().error().msg();
        } else if (rep->reply().status() == odc::ReplyStatus::SUCCESS) {
            std::stringstream ss;
            ss << "GetState response: "
               << "topology state: " << rep->reply().state() << ", sessionId: " << rep->reply().sessionid() << ", partitionId: " << rep->reply().partitionid();
            if (req->detailed()) {
                ss << ", Devices:";
                OLOG(info, common) << ss.str();
                for (const auto& d : rep->devices()) {
                    OLOG(info, common) << "id: " << d.id() << ", state: " << d.state() << ", path: " << d.path() << ", ignored: " << d.ignored() << ", host: " << d.host();
                }
            }
            OLOG(info, common) << ss.str();
        } else {
            OLOG(info, common) << "GetState response: " << rep->DebugString();
        }

        return ::grpc::Status::OK;
    }

    ::grpc::Status SetProperties(::grpc::ServerContext* ctx, const odc::SetPropertiesRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "SetProperties request from " << client << ":\n" << req->DebugString();
        // Convert from protobuf to ODC format
        core::SetPropertiesParams::Properties_t props;
        for (int i = 0; i < req->properties_size(); i++) {
            auto prop{ req->properties(i) };
            props.push_back(core::SetPropertiesParams::Property_t(prop.key(), prop.value()));
        }

        core::SetPropertiesParams setPropertiesParams{ props, req->path() };
        core::RequestResult res{ mController.execSetProperties(common, setPropertiesParams) };
        setupGeneralReply(rep, res);
        logResponse("SetProperties response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Update(::grpc::ServerContext* ctx, const odc::UpdateRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Update request from " << client << ":\n" << req->DebugString();
        core::UpdateParams updateParams{ req->topology(), req->content(), req->script() };
        core::RequestResult res{ mController.execUpdate(common, updateParams) };
        setupGeneralReply(rep, res);
        logResponse("Update response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Configure(::grpc::ServerContext* ctx, const odc::ConfigureRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Configure request from " << client << ":\n" << req->DebugString();
        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execConfigure(common, deviceParams) };
        setupStateReply(rep, res);
        logResponse("Configure response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Start(::grpc::ServerContext* ctx, const odc::StartRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Start request from " << client << ":\n" << req->DebugString();
        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execStart(common, deviceParams) };
        setupStateReply(rep, res);
        logResponse("Start response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Stop(::grpc::ServerContext* ctx, const odc::StopRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Stop request from " << client << ":\n" << req->DebugString();
        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execStop(common, deviceParams) };
        setupStateReply(rep, res);
        logResponse("Stop response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Reset(::grpc::ServerContext* ctx, const odc::ResetRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Reset request from " << client << ":\n" << req->DebugString();
        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execReset(common, deviceParams) };
        setupStateReply(rep, res);
        logResponse("Reset response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Terminate(::grpc::ServerContext* ctx, const odc::TerminateRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Terminate request from " << client << ":\n" << req->DebugString();
        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execTerminate(common, deviceParams) };
        setupStateReply(rep, res);
        logResponse("Terminate response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Shutdown(::grpc::ServerContext* ctx, const odc::ShutdownRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());
        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));
        OLOG(info, common) << "Shutdown request from " << client << ": "
                                 << "partitionId: " << req->partitionid()
                                 << ", runnr: " << req->runnr()
                                 << ", timeout: " << req->timeout();
        core::RequestResult res{ mController.execShutdown(common) };
        setupGeneralReply(rep, res);
        logResponse("Shutdown response:\n", common, rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Status(::grpc::ServerContext* ctx, const odc::StatusRequest* req, odc::StatusReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        OLOG(info) << "Status request from " << client << ": " << req->DebugString();
        core::StatusRequestResult res{ mController.execStatus(core::StatusParams(req->running())) };
        setupStatusReply(rep, res);

        if (rep->status() == odc::ReplyStatus::SUCCESS) {
            OLOG(info) << "Status: found " << rep->partitions().size() << " partition(s)" << (rep->partitions().size() > 0 ? ":" : "");
            for (const auto& p : rep->partitions()) {
                OLOG(info) << "  partitionId: " << p.partitionid()
                           << ", DDS session: " << odc::SessionStatus_Name(p.status())
                           << ", DDS session ID: " << p.sessionid()
                           << ", topology state: " << p.state();
            }
        } else {
            OLOG(error) << "Status: " << rep->DebugString();
        }

        return ::grpc::Status::OK;
    }

  private:
    odc::Error* newError(const core::BaseRequestResult& res)
    {
        odc::Error* error{ new odc::Error() };
        error->set_code(res.mError.mCode.value());
        error->set_msg(res.mError.mCode.message() + " (" + res.mError.mDetails + ")");
        return error;
    }

    void setupGeneralReply(odc::GeneralReply* rep, const core::RequestResult& res)
    {
        if (res.mStatusCode == core::StatusCode::ok) {
            rep->set_status(odc::ReplyStatus::SUCCESS);
            rep->set_msg(res.mMsg);
        } else {
            rep->set_status(odc::ReplyStatus::ERROR);
            rep->set_allocated_error(newError(res));
        }
        rep->set_partitionid(res.mPartitionID);
        rep->set_runnr(res.mRunNr);
        rep->set_sessionid(res.mDDSSessionID);
        rep->set_exectime(res.mExecTime);
        rep->set_state(GetAggregatedStateName(res.mAggregatedState));
    }

    void setupStateReply(odc::StateReply* rep, const core::RequestResult& res)
    {
        // Protobuf message takes the ownership and deletes the object
        odc::GeneralReply* generalResponse{ new odc::GeneralReply() };
        setupGeneralReply(generalResponse, res);
        rep->set_allocated_reply(generalResponse);

        if (res.mDetailedState != nullptr) {
            for (const auto& state : *(res.mDetailedState)) {
                auto device{ rep->add_devices() };
                device->set_id(state.mStatus.taskId);
                device->set_state(fair::mq::GetStateName(state.mStatus.state));
                device->set_path(state.mPath);
                device->set_ignored(state.mStatus.ignored);
                device->set_host(state.mHost);
            }
        }
    }

    void setupStatusReply(odc::StatusReply* rep, const core::StatusRequestResult& res)
    {
        if (res.mStatusCode == core::StatusCode::ok) {
            rep->set_status(odc::ReplyStatus::SUCCESS);
            rep->set_msg(res.mMsg);
        } else {
            rep->set_status(odc::ReplyStatus::ERROR);
            rep->set_allocated_error(newError(res));
        }
        rep->set_exectime(res.mExecTime);
        for (const auto& p : res.mPartitions) {
            auto partition{ rep->add_partitions() };
            partition->set_partitionid(p.mPartitionID);
            partition->set_sessionid(p.mDDSSessionID);
            partition->set_status((p.mDDSSessionStatus == core::DDSSessionStatus::running ? SessionStatus::RUNNING : SessionStatus::STOPPED));
            partition->set_state(GetAggregatedStateName(p.mAggregatedState));
        }
    }

    std::mutex& getMutex(const std::string& partitionID)
    {
        std::lock_guard<std::mutex> lock(mMutexMapMutex);
        auto it{ mMutexMap.find(partitionID) };
        return (it == mMutexMap.end()) ? mMutexMap[partitionID] : it->second;
    }

    template<typename Response_t>
    void logResponse(const std::string& msg, const core::CommonParams& common, const Response_t* res)
    {
        if (res->status() == odc::ReplyStatus::ERROR) {
            OLOG(error, common) << msg << res->DebugString();
        } else {
            OLOG(info, common) << msg << res->DebugString();
        }
    }

    void logResponse(const std::string& msg, const core::CommonParams& common, const odc::StateReply* rep)
    {
        if (rep->reply().status() == odc::ReplyStatus::ERROR) {
            OLOG(error, common) << msg << rep->DebugString();
        } else {
            OLOG(info, common) << msg << rep->DebugString();
        }
    }

    static std::string clientMetadataAsString(const ::grpc::ServerContext& ctx)
    {
        const auto clientMetadata{ ctx.client_metadata() };
        return core::toString("[", ctx.peer(), "] ",
                        std::accumulate(clientMetadata.begin(), clientMetadata.end(), std::string{}, [](std::string prefix, const auto element) {
                            return core::toString(std::move(prefix), prefix.empty() ? "" : ",", element.first, ":", element.second);
                        }));
    }

    core::Controller mController; ///< Core ODC service

    // Mutex for each partition.
    // All requests for a certain partition are processed sequentially.
    std::map<std::string, std::mutex> mMutexMap; ///< Mutex for each partition
    std::mutex mMutexMapMutex;                   ///< Mutex of global mutex map
};

} // namespace odc::grpc

#endif /* defined(ODC_GRPC_CONTROLLER) */
