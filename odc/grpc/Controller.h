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

#include <boost/algorithm/string/split.hpp>

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
    void setHistoryDir(const std::string& dir) { mController.setHistoryDir(dir); }
    void registerResourcePlugins(const core::PluginManager::PluginMap& pluginMap) { mController.registerResourcePlugins(pluginMap); }
    void registerRequestTriggers(const core::PluginManager::PluginMap& triggerMap) { mController.registerRequestTriggers(triggerMap); }
    void restore(const std::string& restoreId, const std::string& restoreDir) { mController.restore(restoreId, restoreDir); }

    ::grpc::Status Initialize(::grpc::ServerContext* ctx, const odc::InitializeRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Initialize", client, common, req);
        OLOG(info, common) << "Initialize request session ID: "   << req->sessionid();

        core::InitializeParams initializeParams{ req->sessionid() };
        core::RequestResult res{ mController.execInitialize(common, initializeParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Initialize", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Submit(::grpc::ServerContext* ctx, const odc::SubmitRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Submit", client, common, req);
        OLOG(info, common) << "Submit request plugin: "   << req->plugin() << ", resources: "   << req->resources();

        core::SubmitParams submitParams{ req->plugin(), req->resources() };
        core::RequestResult res{ mController.execSubmit(common, submitParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Submit", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Activate(::grpc::ServerContext* ctx, const odc::ActivateRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Activate", client, common, req);
        OLOG(info, common) << "Activate request topology file: " << req->topology();
        OLOG(info, common) << "Activate request topology content: "  << req->content();
        if (req->script().empty()) {
            OLOG(info, common) << "Activate request topology script: " << req->script();
        } else {
            OLOG(info, common) << "Activate request topology script (split by ' '):";
            std::vector<std::string> parts;
            boost::split(parts, req->script(), boost::is_any_of(" "));
            for (const auto& part : parts) {
                OLOG(info, common) << part;
            }
            OLOG(info, common) << "Run request END OF TOPOLOGY SCRIPT";
        }

        core::ActivateParams activateParams{ req->topology(), req->content(), req->script() };
        core::RequestResult res{ mController.execActivate(common, activateParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Activate", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Run(::grpc::ServerContext* ctx, const odc::RunRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Run", client, common, req);
        OLOG(info, common) << "Run request plugin: "   << req->plugin() << ", resources: "   << req->resources();
        OLOG(info, common) << "Run request topology file: " << req->topology();
        OLOG(info, common) << "Run request topology content: "  << req->content();
        if (req->script().empty()) {
            OLOG(info, common) << "Run request topology script: " << req->script();
        } else {
            OLOG(info, common) << "Run request topology script (split by ' '):";
            std::vector<std::string> parts;
            boost::split(parts, req->script(), boost::is_any_of(" "));
            for (const auto& part : parts) {
                OLOG(info, common) << part;
            }
            OLOG(info, common) << "Run request END OF TOPOLOGY SCRIPT";
        }

        core::InitializeParams initializeParams{ "" };
        core::SubmitParams submitParams{ req->plugin(), req->resources() };
        core::ActivateParams activateParams{ req->topology(), req->content(), req->script() };
        core::RequestResult res{ mController.execRun(common, initializeParams, submitParams, activateParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Run", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status GetState(::grpc::ServerContext* ctx, const odc::StateRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        logCommonRequest("GetState", client, common, req);
        OLOG(info, common) << "GetState request detailed: " << req->detailed() << ", path: "   << req->path();

        core::DeviceParams deviceParams{ req->path(), req->detailed() };
        core::RequestResult res{ mController.execGetState(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("GetState", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status SetProperties(::grpc::ServerContext* ctx, const odc::SetPropertiesRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("SetProperties", client, common, req);
        OLOG(info, common) << "SetProperties request path: " << req->path();

        // Convert from protobuf to ODC format
        OLOG(info, common) << "SetProperties request properties: ";
        core::SetPropertiesParams::Props props;
        for (int i = 0; i < req->properties_size(); i++) {
            auto prop{ req->properties(i) };
            OLOG(info, common) << "  key: " << prop.key() << ", value: " << prop.value();
            props.push_back(core::SetPropertiesParams::Prop(prop.key(), prop.value()));
        }

        core::SetPropertiesParams setPropertiesParams{ props, req->path() };
        core::RequestResult res{ mController.execSetProperties(common, setPropertiesParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("SetProperties", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Update(::grpc::ServerContext* ctx, const odc::UpdateRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Update", client, common, req);
        OLOG(info, common) << "Update request topology: " << req->topology();
        OLOG(info, common) << "Update request content: "  << req->content();
        OLOG(info, common) << "Update request script: "   << req->script();

        core::UpdateParams updateParams{ req->topology(), req->content(), req->script() };
        core::RequestResult res{ mController.execUpdate(common, updateParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Update", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Configure(::grpc::ServerContext* ctx, const odc::ConfigureRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Configure", client, common, &(req->request()));

        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execConfigure(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Configure", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Start(::grpc::ServerContext* ctx, const odc::StartRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Start", client, common, &(req->request()));

        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execStart(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Start", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Stop(::grpc::ServerContext* ctx, const odc::StopRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Stop", client, common, &(req->request()));

        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execStop(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Stop", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Reset(::grpc::ServerContext* ctx, const odc::ResetRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Reset", client, common, &(req->request()));

        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execReset(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Reset", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Terminate(::grpc::ServerContext* ctx, const odc::TerminateRequest* req, odc::StateReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Terminate", client, common, &(req->request()));

        core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        core::RequestResult res{ mController.execTerminate(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Terminate", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Shutdown(::grpc::ServerContext* ctx, const odc::ShutdownRequest* req, odc::GeneralReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Shutdown", client, common, req);

        core::RequestResult res{ mController.execShutdown(common) };

        setupGeneralReply(rep, res);
        logGeneralReply("Shutdown", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Status(::grpc::ServerContext* ctx, const odc::StatusRequest* req, odc::StatusReply* rep)
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };

        OLOG(info) << "Status request from " << client << ": runnning: " << req->running();

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

    template<typename Request>
    void logCommonRequest(const std::string& label, const std::string& client, const core::CommonParams& common, const Request* req)
    {
        OLOG(info, common) << label << " request from " << client << ": "
            << "partitionId: " << req->partitionid()
            << ", runnr: "     << req->runnr()
            << ", timeout: "   << req->timeout();
    }

    void logGeneralReply(const std::string& label, const core::CommonParams& common, const GeneralReply& rep)
    {
        std::stringstream ss;
        ss << label << " reply: "
           << "partitionId: " << rep.partitionid()
           << ", runnr: "     << rep.runnr()
           << ", sessionid: " << rep.sessionid()
           << ", state: "     << rep.state()
           << ", msg: "       << rep.msg()
           << ", exectime: "  << rep.exectime() << "ms";

        if (rep.status() == odc::ReplyStatus::ERROR) {
            ss << ", ERROR: " << rep.error().msg() << " (" << rep.error().code() << ") ";
            OLOG(error, common) << ss.str();
        } else if (rep.status() == odc::ReplyStatus::SUCCESS) {
            OLOG(info, common) << ss.str();
        } else {
            OLOG(error, common) << label << " reply: " << rep.DebugString();
        }
    }

    void logStateReply(const std::string& label, const core::CommonParams& common, const StateReply& rep)
    {
        logGeneralReply(label, common, rep.reply());
        if (!rep.devices().empty()) {
            OLOG(info, common) << "Detailed list of devices:";
            for (const auto& d : rep.devices()) {
                OLOG(info, common) << "id: "        << d.id()
                                   << ", state: "   << d.state()
                                   << ", path: "    << d.path()
                                   << ", ignored: " << d.ignored()
                                   << ", host: "    << d.host();
            }
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
