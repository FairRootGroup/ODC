/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <cassert>
#include <numeric>
#include <sstream>
#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/Topology.h>
#include <odc/grpc/GrpcService.h>
#include <string>

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

namespace {

std::string clientMetadataAsString(const ::grpc::ServerContext& ctx)
{
    const auto clientMetadata{ ctx.client_metadata() };
    return toString("[", ctx.peer(), "]{",
                    std::accumulate(clientMetadata.begin(),
                                    clientMetadata.end(),
                                    std::string{},
                                    [](std::string prefix, const auto element) { return toString(std::move(prefix), prefix.empty() ? "" : ",", element.first, ":", element.second); }),
                    "}");
}

} // namespace

void CGrpcService::setTimeout(const std::chrono::seconds& timeout) { mService.setTimeout(timeout); }
void CGrpcService::registerResourcePlugins(const CPluginManager::PluginMap_t& pluginMap) { mService.registerResourcePlugins(pluginMap); }
void CGrpcService::registerRequestTriggers(const CPluginManager::PluginMap_t& triggerMap) { mService.registerRequestTriggers(triggerMap); }
void CGrpcService::restore(const std::string& restoreId) { mService.restore(restoreId); }

::grpc::Status CGrpcService::Initialize(::grpc::ServerContext* ctx, const odc::InitializeRequest* req, odc::GeneralReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Initialize request from " << client << ":\n" << req->DebugString();
    SInitializeParams initializeParams{ req->sessionid() };
    RequestResult res{ mService.execInitialize(commonParams, initializeParams) };
    setupGeneralReply(rep, res);
    logResponse("Initialize response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Submit(::grpc::ServerContext* ctx, const odc::SubmitRequest* req, odc::GeneralReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Submit request from " << client << ":\n" << req->DebugString();
    SSubmitParams submitParams{ req->plugin(), req->resources() };
    RequestResult res{ mService.execSubmit(commonParams, submitParams) };
    setupGeneralReply(rep, res);
    logResponse("Submit response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Activate(::grpc::ServerContext* ctx, const odc::ActivateRequest* req, odc::GeneralReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Activate request from " << client << ":\n" << req->DebugString();
    SActivateParams activateParams{ req->topology(), req->content(), req->script() };
    RequestResult res{ mService.execActivate(commonParams, activateParams) };
    setupGeneralReply(rep, res);
    logResponse("Activate response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Run(::grpc::ServerContext* ctx, const odc::RunRequest* req, odc::GeneralReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Run request from " << client << ":\n" << req->DebugString();
    SInitializeParams initializeParams{ "" };
    SSubmitParams submitParams{ req->plugin(), req->resources() };
    SActivateParams activateParams{ req->topology(), req->content(), req->script() };
    RequestResult res{ mService.execRun(commonParams, initializeParams, submitParams, activateParams) };
    setupGeneralReply(rep, res);
    logResponse("Run response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Update(::grpc::ServerContext* ctx, const odc::UpdateRequest* req, odc::GeneralReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Update request from " << client << ":\n" << req->DebugString();
    SUpdateParams updateParams{ req->topology(), req->content(), req->script() };
    RequestResult res{ mService.execUpdate(commonParams, updateParams) };
    setupGeneralReply(rep, res);
    logResponse("Update response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::GetState(::grpc::ServerContext* ctx, const odc::StateRequest* req, odc::StateReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "GetState request from " << client << ":\n" << req->DebugString();
    SDeviceParams deviceParams{ req->path(), req->detailed() };
    RequestResult res{ mService.execGetState(commonParams, deviceParams) };
    setupStateReply(rep, res);

    if (rep->reply().status() == odc::ReplyStatus::ERROR) {
        OLOG(error, commonParams) << "GetState response:" << " ERROR"
            << " (" << rep->reply().error().code()
            << "), sessionId: " << rep->reply().sessionid()
            << ", partitionId: " << rep->reply().partitionid()
            << ", state: " << rep->reply().state()
            << ", msg: " << rep->reply().error().msg();
    } else if (rep->reply().status() == odc::ReplyStatus::SUCCESS) {
        stringstream ss;
        ss << "GetState response: "
           << "state: " << rep->reply().state()
           << ", sessionId: " << rep->reply().sessionid()
           << ", partitionId: " << rep->reply().partitionid();
        if (req->detailed()) {
            ss << ", Devices:\n";
            for (const auto& d : rep->devices()) {
                ss << "id: " << d.id() << ", state: " << d.state() << ", path: " << d.path() << "\n";
            }
        }
        OLOG(info, commonParams) << ss.str();
    } else {
        OLOG(info, commonParams) << "GetState response: " << rep->DebugString();
    }

    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::SetProperties(::grpc::ServerContext* ctx, const odc::SetPropertiesRequest* req, odc::GeneralReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "SetProperties request from " << client << ":\n" << req->DebugString();
    // Convert from protobuf to ODC format
    SetPropertiesParams::Properties_t props;
    for (int i = 0; i < req->properties_size(); i++) {
        auto prop{ req->properties(i) };
        props.push_back(SetPropertiesParams::Property_t(prop.key(), prop.value()));
    }

    SetPropertiesParams setPropertiesParams{ props, req->path() };
    RequestResult res{ mService.execSetProperties(commonParams, setPropertiesParams) };
    setupGeneralReply(rep, res);
    logResponse("SetProperties response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Configure(::grpc::ServerContext* ctx, const odc::ConfigureRequest* req, odc::StateReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->request().partitionid(), req->request().runnr(), req->request().timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Configure request from " << client << ":\n" << req->DebugString();
    SDeviceParams deviceParams{ req->request().path(), req->request().detailed() };
    RequestResult res{ mService.execConfigure(commonParams, deviceParams) };
    setupStateReply(rep, res);
    logResponse("Configure response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Start(::grpc::ServerContext* ctx, const odc::StartRequest* req, odc::StateReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->request().partitionid(), req->request().runnr(), req->request().timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Start request from " << client << ":\n" << req->DebugString();
    SDeviceParams deviceParams{ req->request().path(), req->request().detailed() };
    RequestResult res{ mService.execStart(commonParams, deviceParams) };
    setupStateReply(rep, res);
    logResponse("Start response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Stop(::grpc::ServerContext* ctx, const odc::StopRequest* req, odc::StateReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->request().partitionid(), req->request().runnr(), req->request().timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Stop request from " << client << ":\n" << req->DebugString();
    SDeviceParams deviceParams{ req->request().path(), req->request().detailed() };
    RequestResult res{ mService.execStop(commonParams, deviceParams) };
    setupStateReply(rep, res);
    logResponse("Stop response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Reset(::grpc::ServerContext* ctx, const odc::ResetRequest* req, odc::StateReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->request().partitionid(), req->request().runnr(), req->request().timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Reset request from " << client << ":\n" << req->DebugString();
    SDeviceParams deviceParams{ req->request().path(), req->request().detailed() };
    RequestResult res{ mService.execReset(commonParams, deviceParams) };
    setupStateReply(rep, res);
    logResponse("Reset response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Terminate(::grpc::ServerContext* ctx, const odc::TerminateRequest* req, odc::StateReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->request().partitionid(), req->request().runnr(), req->request().timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Terminate request from " << client << ":\n" << req->DebugString();
    SDeviceParams deviceParams{ req->request().path(), req->request().detailed() };
    RequestResult res{ mService.execTerminate(commonParams, deviceParams) };
    setupStateReply(rep, res);
    logResponse("Terminate response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Shutdown(::grpc::ServerContext* ctx, const odc::ShutdownRequest* req, odc::GeneralReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    const CommonParams commonParams(req->partitionid(), req->runnr(), req->timeout());
    lock_guard<mutex> lock(getMutex(commonParams.m_partitionID));
    OLOG(info, commonParams) << "Shutdown request from " << client << ":\n" << req->DebugString();
    RequestResult res{ mService.execShutdown(commonParams) };
    setupGeneralReply(rep, res);
    logResponse("Shutdown response:\n", commonParams, rep);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Status(::grpc::ServerContext* ctx, const odc::StatusRequest* req, odc::StatusReply* rep)
{
    assert(ctx);
    const string client{ clientMetadataAsString(*ctx) };
    OLOG(info) << "Status request from " << client << ":\n" << req->DebugString();
    StatusRequestResult res{ mService.execStatus(SStatusParams(req->running())) };
    setupStatusReply(rep, res);
    logResponse("Status response:\n", core::CommonParams(), rep);
    return ::grpc::Status::OK;
}

odc::Error* CGrpcService::newError(const BaseRequestResult& res)
{
    odc::Error* error{ new odc::Error() };
    error->set_code(res.m_error.m_code.value());
    error->set_msg(res.m_error.m_code.message() + " (" + res.m_error.m_details + ")");
    return error;
}

void CGrpcService::setupGeneralReply(odc::GeneralReply* rep, const RequestResult& res)
{
    if (res.m_statusCode == StatusCode::ok) {
        rep->set_status(odc::ReplyStatus::SUCCESS);
        rep->set_msg(res.m_msg);
    } else {
        rep->set_status(odc::ReplyStatus::ERROR);
        rep->set_allocated_error(newError(res));
    }
    rep->set_partitionid(res.m_partitionID);
    rep->set_runnr(res.m_runNr);
    rep->set_sessionid(res.m_sessionID);
    rep->set_exectime(res.m_execTime);
    rep->set_state(GetAggregatedStateName(res.m_aggregatedState));
}

void CGrpcService::setupStateReply(odc::StateReply* rep, const odc::core::RequestResult& res)
{
    // Protobuf message takes the ownership and deletes the object
    odc::GeneralReply* generalResponse{ new odc::GeneralReply() };
    setupGeneralReply(generalResponse, res);
    rep->set_allocated_reply(generalResponse);

    if (res.mFullState != nullptr) {
        for (const auto& state : *(res.mFullState)) {
            auto device{ rep->add_devices() };
            device->set_path(state.m_path);
            device->set_id(state.m_status.taskId);
            device->set_state(fair::mq::GetStateName(state.m_status.state));
        }
    }
}

void CGrpcService::setupStatusReply(odc::StatusReply* rep, const odc::core::StatusRequestResult& res)
{
    if (res.m_statusCode == StatusCode::ok) {
        rep->set_status(odc::ReplyStatus::SUCCESS);
        rep->set_msg(res.m_msg);
    } else {
        rep->set_status(odc::ReplyStatus::ERROR);
        rep->set_allocated_error(newError(res));
    }
    rep->set_exectime(res.m_execTime);
    for (const auto& p : res.m_partitions) {
        auto partition{ rep->add_partitions() };
        partition->set_partitionid(p.m_partitionID);
        partition->set_sessionid(p.m_sessionID);
        partition->set_status((p.m_sessionStatus == DDSSessionStatus::running ? SessionStatus::RUNNING : SessionStatus::STOPPED));
        partition->set_state(GetAggregatedStateName(p.m_aggregatedState));
    }
}

std::mutex& CGrpcService::getMutex(const std::string& partitionID)
{
    std::lock_guard<std::mutex> lock(mMutexMapMutex);
    auto it{ mMutexMap.find(partitionID) };
    return (it == mMutexMap.end()) ? mMutexMap[partitionID] : it->second;
}

template<typename Response_t>
void CGrpcService::logResponse(const string& msg, const core::CommonParams& commonParams, const Response_t* res)
{
    if (res->status() == odc::ReplyStatus::ERROR) {
        OLOG(error, commonParams) << msg << res->DebugString();
    } else {
        OLOG(info, commonParams) << msg << res->DebugString();
    }
}

void CGrpcService::logResponse(const std::string& msg, const core::CommonParams& commonParams, const odc::StateReply* rep)
{
    if (rep->reply().status() == odc::ReplyStatus::ERROR) {
        OLOG(error, commonParams) << msg << rep->DebugString();
    } else {
        OLOG(info, commonParams) << msg << rep->DebugString();
    }
}
