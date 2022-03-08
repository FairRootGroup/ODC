/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <cassert>
#include <numeric>
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

auto clientMetadataAsString(const ::grpc::ServerContext& context) -> std::string
{
    const auto clientMetadata{ context.client_metadata() };
    return toString("[", context.peer(), "]{",
                    std::accumulate(clientMetadata.begin(),
                                    clientMetadata.end(),
                                    std::string{},
                                    [](std::string prefix, const auto element) { return toString(std::move(prefix), prefix.empty() ? "" : ",", element.first, ":", element.second); }),
                    "}");
}

} // namespace

void CGrpcService::setTimeout(const std::chrono::seconds& _timeout) { m_service.setTimeout(_timeout); }

void CGrpcService::registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap) { m_service.registerResourcePlugins(_pluginMap); }

void CGrpcService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap) { m_service.registerRequestTriggers(_triggerMap); }

void CGrpcService::restore(const std::string& _restoreId) { m_service.restore(_restoreId); }

::grpc::Status CGrpcService::Initialize(::grpc::ServerContext* context, const odc::InitializeRequest* request, odc::GeneralReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Initialize request from " << client << ":\n" << request->DebugString();
    SInitializeParams params{ request->sessionid() };
    RequestResult result{ m_service.execInitialize(common, params) };
    setupGeneralReply(response, result);
    logResponse("Initialize response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Submit(::grpc::ServerContext* context, const odc::SubmitRequest* request, odc::GeneralReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Submit request from " << client << ":\n" << request->DebugString();
    SSubmitParams params{ request->plugin(), request->resources() };
    RequestResult result{ m_service.execSubmit(common, params) };
    setupGeneralReply(response, result);
    logResponse("Submit response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Activate(::grpc::ServerContext* context, const odc::ActivateRequest* request, odc::GeneralReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Activate request from " << client << ":\n" << request->DebugString();
    SActivateParams params{ request->topology(), request->content(), request->script() };
    RequestResult result{ m_service.execActivate(common, params) };
    setupGeneralReply(response, result);
    logResponse("Activate response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Run(::grpc::ServerContext* context, const odc::RunRequest* request, odc::GeneralReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Run request from " << client << ":\n" << request->DebugString();
    SInitializeParams initializeParams{ "" };
    SSubmitParams submitParams{ request->plugin(), request->resources() };
    SActivateParams activateParams{ request->topology(), request->content(), request->script() };
    RequestResult result{ m_service.execRun(common, initializeParams, submitParams, activateParams) };
    setupGeneralReply(response, result);
    logResponse("Run response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Update(::grpc::ServerContext* context, const odc::UpdateRequest* request, odc::GeneralReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Update request from " << client << ":\n" << request->DebugString();
    SUpdateParams params{ request->topology(), request->content(), request->script() };
    RequestResult result{ m_service.execUpdate(common, params) };
    setupGeneralReply(response, result);
    logResponse("Update response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::GetState(::grpc::ServerContext* context, const odc::StateRequest* request, odc::StateReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "GetState request from " << client << ":\n" << request->DebugString();
    SDeviceParams params{ request->path(), request->detailed() };
    RequestResult result{ m_service.execGetState(common, params) };
    setupStateReply(response, result);
    logResponse("GetState response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::SetProperties(::grpc::ServerContext* context, const odc::SetPropertiesRequest* request, odc::GeneralReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "SetProperties request from " << client << ":\n" << request->DebugString();
    // Convert from protobuf to ODC format
    SSetPropertiesParams::Properties_t props;
    for (int i = 0; i < request->properties_size(); i++) {
        auto prop{ request->properties(i) };
        props.push_back(SSetPropertiesParams::Property_t(prop.key(), prop.value()));
    }

    SSetPropertiesParams params{ props, request->path() };
    RequestResult result{ m_service.execSetProperties(common, params) };
    setupGeneralReply(response, result);
    logResponse("SetProperties response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Configure(::grpc::ServerContext* context, const odc::ConfigureRequest* request, odc::StateReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Configure request from " << client << ":\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    RequestResult result{ m_service.execConfigure(common, params) };
    setupStateReply(response, result);
    logResponse("Configure response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Start(::grpc::ServerContext* context, const odc::StartRequest* request, odc::StateReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Start request from " << client << ":\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    RequestResult result{ m_service.execStart(common, params) };
    setupStateReply(response, result);
    logResponse("Start response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Stop(::grpc::ServerContext* context, const odc::StopRequest* request, odc::StateReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Stop request from " << client << ":\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    RequestResult result{ m_service.execStop(common, params) };
    setupStateReply(response, result);
    logResponse("Stop response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Reset(::grpc::ServerContext* context, const odc::ResetRequest* request, odc::StateReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Reset request from " << client << ":\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    RequestResult result{ m_service.execReset(common, params) };
    setupStateReply(response, result);
    logResponse("Reset response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Terminate(::grpc::ServerContext* context, const odc::TerminateRequest* request, odc::StateReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Terminate request from " << client << ":\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    RequestResult result{ m_service.execTerminate(common, params) };
    setupStateReply(response, result);
    logResponse("Terminate response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Shutdown(::grpc::ServerContext* context, const odc::ShutdownRequest* request, odc::GeneralReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(info, common) << "Shutdown request from " << client << ":\n" << request->DebugString();
    RequestResult result{ m_service.execShutdown(common) };
    setupGeneralReply(response, result);
    logResponse("Shutdown response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Status(::grpc::ServerContext* context, const odc::StatusRequest* request, odc::StatusReply* response)
{
    assert(context);
    const auto client{ clientMetadataAsString(*context) };
    OLOG(info) << "Status request from " << client << ":\n" << request->DebugString();
    StatusRequestResult result{ m_service.execStatus(SStatusParams(request->running())) };
    setupStatusReply(response, result);
    logResponse("Status response:\n", core::SCommonParams(), response);
    return ::grpc::Status::OK;
}

odc::Error* CGrpcService::newError(const BaseRequestResult& result)
{
    odc::Error* error{ new odc::Error() };
    error->set_code(result.m_error.m_code.value());
    error->set_msg(result.m_error.m_code.message() + " (" + result.m_error.m_details + ")");
    return error;
}

void CGrpcService::setupGeneralReply(odc::GeneralReply* _response, const RequestResult& result)
{
    if (result.m_statusCode == EStatusCode::ok) {
        _response->set_status(odc::ReplyStatus::SUCCESS);
        _response->set_msg(result.m_msg);
    } else {
        _response->set_status(odc::ReplyStatus::ERROR);
        _response->set_allocated_error(newError(result));
    }
    _response->set_partitionid(result.m_partitionID);
    _response->set_runnr(result.m_runNr);
    _response->set_sessionid(result.m_sessionID);
    _response->set_exectime(result.m_execTime);
    _response->set_state(GetAggregatedTopologyStateName(result.m_aggregatedState));
}

void CGrpcService::setupStateReply(odc::StateReply* _response, const odc::core::RequestResult& result)
{
    // Protobuf message takes the ownership and deletes the object
    odc::GeneralReply* generalResponse{ new odc::GeneralReply() };
    setupGeneralReply(generalResponse, result);
    _response->set_allocated_reply(generalResponse);

    if (result.mFullState != nullptr) {
        for (const auto& state : *(result.mFullState)) {
            auto device{ _response->add_devices() };
            device->set_path(state.m_path);
            device->set_id(state.m_status.taskId);
            device->set_state(fair::mq::GetStateName(state.m_status.state));
        }
    }
}

void CGrpcService::setupStatusReply(odc::StatusReply* _response, const odc::core::StatusRequestResult& result)
{
    if (result.m_statusCode == EStatusCode::ok) {
        _response->set_status(odc::ReplyStatus::SUCCESS);
        _response->set_msg(result.m_msg);
    } else {
        _response->set_status(odc::ReplyStatus::ERROR);
        _response->set_allocated_error(newError(result));
    }
    _response->set_exectime(result.m_execTime);
    for (const auto& p : result.m_partitions) {
        auto partition{ _response->add_partitions() };
        partition->set_partitionid(p.m_partitionID);
        partition->set_sessionid(p.m_sessionID);
        partition->set_status((p.m_sessionStatus == ESessionStatus::running ? SessionStatus::RUNNING : SessionStatus::STOPPED));
        partition->set_state(GetAggregatedTopologyStateName(p.m_aggregatedState));
    }
}

std::mutex& CGrpcService::getMutex(const std::string& _partitionID)
{
    std::lock_guard<std::mutex> lock(m_mutexMapMutex);
    auto it{ m_mutexMap.find(_partitionID) };
    return (it == m_mutexMap.end()) ? m_mutexMap[_partitionID] : it->second;
}

template<typename Request_t>
core::SCommonParams CGrpcService::commonParams(const Request_t* _request)
{
    return core::SCommonParams(_request->partitionid(), _request->runnr(), _request->timeout());
}

template<typename Response_t>
void CGrpcService::logResponse(const string& _msg, const core::SCommonParams& _common, const Response_t* _response)
{
    if (_response->status() == odc::ReplyStatus::ERROR) {
        OLOG(error, _common) << _msg << _response->DebugString();
    } else {
        OLOG(info, _common) << _msg << _response->DebugString();
    }
}

void CGrpcService::logResponse(const std::string& _msg, const core::SCommonParams& _common, const odc::StateReply* _response)
{
    if (_response->reply().status() == odc::ReplyStatus::ERROR) {
        OLOG(error, _common) << _msg << _response->DebugString();
    } else {
        OLOG(info, _common) << _msg << _response->DebugString();
    }
}
