/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_GRPCCONTROLLER
#define ODC_GRPCCONTROLLER

#include <odc/DDSSubmit.h>
#include <odc/Controller.h>
#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/PluginManager.h>
#include <odc/Topology.h>
#include <odc/Version.h>

#include <dds/Version.h>

#include <grpcpp/grpcpp.h>
#include <odc/grpc/odc.grpc.pb.h>

#include <boost/algorithm/string/split.hpp>

#include <cassert>
#include <numeric>
#include <sstream>
#include <string>

namespace odc {

class GrpcController final : public odc::ODC::Service
{
  public:
    GrpcController() {}

    void run(const std::string& host)
    {
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(host, ::grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
        server->Wait();
    }

    void setTimeout(const std::chrono::seconds& timeout) { mController.setTimeout(timeout); }
    void setHistoryDir(const std::string& dir) { mController.setHistoryDir(dir); }
    void setZoneCfgs(const std::vector<std::string>& zonesStr) { mController.setZoneCfgs(zonesStr); }
    void setRMS(const std::string& rms) { mController.setRMS(rms); }

    void registerResourcePlugins(const core::PluginManager::PluginMap& pluginMap) { mController.registerResourcePlugins(pluginMap); }
    void restore(const std::string& restoreId, const std::string& restoreDir) { mController.restore(restoreId, restoreDir); }

  private:
    ::grpc::Status Initialize(::grpc::ServerContext* ctx, const odc::InitializeRequest* req, odc::GeneralReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Initialize", client, common, req);
        OLOG(info, common) << "Initialize request session ID: " << req->sessionid();

        const core::InitializeParams initializeParams{ req->sessionid() };
        const core::RequestResult res{ mController.execInitialize(common, initializeParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Initialize", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Submit(::grpc::ServerContext* ctx, const odc::SubmitRequest* req, odc::GeneralReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Submit", client, common, req);
        OLOG(info, common) << "Submit request plugin: " << req->plugin() << "; resources: " << req->resources();

        const core::SubmitParams submitParams{ req->plugin(), req->resources() };
        const core::RequestResult res{ mController.execSubmit(common, submitParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Submit", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Activate(::grpc::ServerContext* ctx, const odc::ActivateRequest* req, odc::GeneralReply* rep) override
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

        const core::ActivateParams activateParams{ req->topology(), req->content(), req->script() };
        const core::RequestResult res{ mController.execActivate(common, activateParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Activate", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Run(::grpc::ServerContext* ctx, const odc::RunRequest* req, odc::GeneralReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Run", client, common, req);
        OLOG(info, common) << "Run request plugin: " << req->plugin()
                           << "; resources: " << req->resources()
                           << "; extractTopoResources: " << req->extracttoporesources();
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

        const core::RunParams runParams{ req->plugin(), req->resources(), req->topology(), req->content(), req->script(), req->extracttoporesources() };
        const core::RequestResult res{ mController.execRun(common, runParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Run", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Update(::grpc::ServerContext* ctx, const odc::UpdateRequest* req, odc::GeneralReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Update", client, common, req);
        OLOG(info, common) << "Update request topology: " << req->topology();
        OLOG(info, common) << "Update request content: "  << req->content();
        OLOG(info, common) << "Update request script: "   << req->script();

        const core::UpdateParams updateParams{ req->topology(), req->content(), req->script() };
        const core::RequestResult res{ mController.execUpdate(common, updateParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("Update", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status GetState(::grpc::ServerContext* ctx, const odc::StateRequest* req, odc::StateReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        logCommonRequest("GetState", client, common, req, true);
        OLOG(debug, common) << "GetState request detailed: " << req->detailed() << "; path: "   << req->path();

        const core::DeviceParams deviceParams{ req->path(), req->detailed() };
        const core::RequestResult res{ mController.execGetState(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("GetState", common, *rep, true);
        return ::grpc::Status::OK;
    }

    ::grpc::Status SetProperties(::grpc::ServerContext* ctx, const odc::SetPropertiesRequest* req, odc::GeneralReply* rep) override
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
            OLOG(info, common) << "  key: " << prop.key() << "; value: " << prop.value();
            props.push_back(core::SetPropertiesParams::Prop(prop.key(), prop.value()));
        }

        const core::SetPropertiesParams setPropertiesParams{ props, req->path() };
        const core::RequestResult res{ mController.execSetProperties(common, setPropertiesParams) };

        setupGeneralReply(rep, res);
        logGeneralReply("SetProperties", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Configure(::grpc::ServerContext* ctx, const odc::ConfigureRequest* req, odc::StateReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Configure", client, common, &(req->request()));

        const core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        const core::RequestResult res{ mController.execConfigure(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Configure", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Start(::grpc::ServerContext* ctx, const odc::StartRequest* req, odc::StateReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Start", client, common, &(req->request()));

        const core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        const core::RequestResult res{ mController.execStart(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Start", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Stop(::grpc::ServerContext* ctx, const odc::StopRequest* req, odc::StateReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Stop", client, common, &(req->request()));

        const core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        const core::RequestResult res{ mController.execStop(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Stop", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Reset(::grpc::ServerContext* ctx, const odc::ResetRequest* req, odc::StateReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Reset", client, common, &(req->request()));

        const core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        const core::RequestResult res{ mController.execReset(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Reset", common, *rep);
        return ::grpc::Status::OK;
    }
    ::grpc::Status Terminate(::grpc::ServerContext* ctx, const odc::TerminateRequest* req, odc::StateReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->request().partitionid(), req->request().runnr(), req->request().timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Terminate", client, common, &(req->request()));

        const core::DeviceParams deviceParams{ req->request().path(), req->request().detailed() };
        const core::RequestResult res{ mController.execTerminate(common, deviceParams) };

        setupStateReply(rep, res);
        logStateReply("Terminate", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Shutdown(::grpc::ServerContext* ctx, const odc::ShutdownRequest* req, odc::GeneralReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };
        const core::CommonParams common(req->partitionid(), req->runnr(), req->timeout());

        std::lock_guard<std::mutex> lock(getMutex(common.mPartitionID));

        logCommonRequest("Shutdown", client, common, req);

        const core::RequestResult res{ mController.execShutdown(common) };

        setupGeneralReply(rep, res);
        logGeneralReply("Shutdown", common, *rep);
        return ::grpc::Status::OK;
    }

    ::grpc::Status Status(::grpc::ServerContext* ctx, const odc::StatusRequest* req, odc::StatusReply* rep) override
    {
        assert(ctx);
        const std::string client{ clientMetadataAsString(*ctx) };

        OLOG(info) << "Status request for ODC " << ODC_VERSION << " (DDS " << DDS_VERSION_STRING << ") from " << client << ": runnning: " << req->running();

        const core::StatusRequestResult res{ mController.execStatus(core::StatusParams(req->running())) };
        setupStatusReply(rep, res);
        logStatusReply(*rep);
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
        rep->set_state(GetAggregatedStateName(res.mTopologyState.aggregated));
        for (const auto& host : res.mHosts) {
            rep->add_hosts(host);
        }
    }

    void setupStateReply(odc::StateReply* rep, const core::RequestResult& res)
    {
        // Protobuf message takes the ownership and deletes the object
        odc::GeneralReply* generalResponse{ new odc::GeneralReply() };
        setupGeneralReply(generalResponse, res);
        rep->set_allocated_reply(generalResponse);

        if (res.mTopologyState.detailed.has_value()) {
            for (const auto& state : res.mTopologyState.detailed.value()) {
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
    void logCommonRequest(const std::string& label, const std::string& client, const core::CommonParams& common, const Request* req, bool debugLog = false)
    {
        std::stringstream ss;
        ss << label << " request for ODC " << ODC_VERSION << " (DDS " << DDS_VERSION_STRING << ") from " << client << ": "
            << "partitionId: " << req->partitionid()
            << "; runnr: "     << req->runnr()
            << "; timeout: "   << req->timeout();
        if (debugLog) {
            OLOG(debug, common) << ss.str();
        } else {
            OLOG(info, common) << ss.str();
        }
    }

    void logGeneralReply(const std::string& label, const core::CommonParams& common, const GeneralReply& rep, bool debugLog = false, bool noError = false)
    {
        std::stringstream ss;
        ss << label << " reply: "
           << "partitionId: " << rep.partitionid()
           << "; runnr: "     << rep.runnr()
           << "; sessionid: " << rep.sessionid()
           << "; state: "     << rep.state()
           << "; msg: "       << rep.msg()
           << "; exectime: "  << rep.exectime() << "ms";
        if (!rep.hosts().empty()) {
            ss << "; hosts: ";
            for (int i = 0; i < rep.hosts().size(); ++i) {
                ss << rep.hosts().at(i) << (i == (rep.hosts().size() - 1) ? "" : ", ");
            }
        }

        if (rep.status() == odc::ReplyStatus::ERROR) {
            ss << "; ERROR: " << rep.error().msg() << " (" << rep.error().code() << ") ";
            if (debugLog) {
                OLOG(debug, common) << ss.str();
            } else if (noError) {
                OLOG(info, common) << ss.str();
            } else {
                OLOG(error, common) << ss.str();
            }
        } else if (rep.status() == odc::ReplyStatus::SUCCESS) {
            if (debugLog) {
                OLOG(info, common) << ss.str();
            } else {
                OLOG(info, common) << ss.str();
            }
        } else {
            if (debugLog) {
                OLOG(debug, common) << label << " reply: " << rep.DebugString();
            } else if (noError) {
                OLOG(info, common) << label << " reply: " << rep.DebugString();
            } else {
                OLOG(error, common) << label << " reply: " << rep.DebugString();
            }
        }
    }

    void logStateReply(const std::string& label, const core::CommonParams& common, const StateReply& rep, bool debugLog = false)
    {
        logGeneralReply(label, common, rep.reply(), debugLog, true);
        if (!rep.devices().empty()) {
            OLOG(debug, common) << "Detailed list of " << rep.devices().size() << " devices:";
            for (const auto& d : rep.devices()) {
                OLOG(debug, common) << "id: "        << d.id()
                                   << "; state: "   << d.state()
                                   << "; path: "    << d.path()
                                   << "; ignored: " << d.ignored()
                                   << "; host: "    << d.host();
            }
        }
    }

    void logStatusReply(const odc::StatusReply& rep)
    {
        if (rep.status() == odc::ReplyStatus::SUCCESS) {
            OLOG(info) << "Status: found " << rep.partitions().size() << " partition(s)" << (rep.partitions().size() > 0 ? ":" : "");
            for (const auto& p : rep.partitions()) {
                OLOG(info) << "  partitionId: " << p.partitionid()
                           << "; DDS session: " << odc::SessionStatus_Name(p.status())
                           << "; DDS session ID: " << p.sessionid()
                           << "; Run Nr.: " << p.runnr()
                           << "; topology state: " << p.state();
            }
        } else {
            OLOG(error) << "Status: " << rep.DebugString();
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

    // Mutex for each partition - all requests for a certain partition are processed sequentially.
    std::map<std::string, std::mutex> mMutexMap; ///< Mutex for each partition
    std::mutex mMutexMapMutex;                   ///< Mutex of global mutex map
};

} // namespace odc::grpc

#endif // ODC_GRPCCONTROLLER
