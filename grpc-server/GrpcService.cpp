// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "GrpcService.h"
#include "Logger.h"
#include "Topology.h"

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

CGrpcService::CGrpcService()
    : m_service(make_shared<CControlService>())
{
}

void CGrpcService::setTimeout(const std::chrono::seconds& _timeout)
{
    m_service->setTimeout(_timeout);
}

void CGrpcService::registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap)
{
    m_service->registerResourcePlugins(_pluginMap);
}

void CGrpcService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap)
{
    m_service->registerRequestTriggers(_triggerMap);
}

void CGrpcService::restore(const std::string& _restoreId)
{
    m_service->restore(_restoreId);
}

::grpc::Status CGrpcService::Initialize(::grpc::ServerContext* /*context*/,
                                        const odc::InitializeRequest* request,
                                        odc::GeneralReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Initialize request:\n" << request->DebugString();
    SInitializeParams params{ request->sessionid() };
    SReturnValue value{ m_service->execInitialize(common, params) };
    setupGeneralReply(response, value);
    logResponse("Initialize response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Submit(::grpc::ServerContext* /*context*/,
                                    const odc::SubmitRequest* request,
                                    odc::GeneralReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Submit request:\n" << request->DebugString();
    SSubmitParams params{ request->plugin(), request->resources() };
    SReturnValue value{ m_service->execSubmit(common, params) };
    setupGeneralReply(response, value);
    logResponse("Submit response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Activate(::grpc::ServerContext* /*context*/,
                                      const odc::ActivateRequest* request,
                                      odc::GeneralReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Activate request:\n" << request->DebugString();
    SActivateParams params{ request->topology(), request->content(), request->script() };
    SReturnValue value{ m_service->execActivate(common, params) };
    setupGeneralReply(response, value);
    logResponse("Activate response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Run(::grpc::ServerContext* /*context*/,
                                 const odc::RunRequest* request,
                                 odc::GeneralReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Run request:\n" << request->DebugString();
    SInitializeParams initializeParams{ "" };
    SSubmitParams submitParams{ request->plugin(), request->resources() };
    SActivateParams activateParams{ request->topology(), request->content(), request->script() };
    SReturnValue value{ m_service->execRun(common, initializeParams, submitParams, activateParams) };
    setupGeneralReply(response, value);
    logResponse("Run response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Update(::grpc::ServerContext* /*context*/,
                                    const odc::UpdateRequest* request,
                                    odc::GeneralReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Update request:\n" << request->DebugString();
    SUpdateParams params{ request->topology(), request->content(), request->script() };
    SReturnValue value{ m_service->execUpdate(common, params) };
    setupGeneralReply(response, value);
    logResponse("Update response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::GetState(::grpc::ServerContext* /*context*/,
                                      const odc::StateRequest* request,
                                      odc::StateReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "GetState request:\n" << request->DebugString();
    SDeviceParams params{ request->path(), request->detailed() };
    SReturnValue value{ m_service->execGetState(common, params) };
    setupStateReply(response, value);
    logResponse("GetState response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::SetProperties(::grpc::ServerContext* /*context*/,
                                           const odc::SetPropertiesRequest* request,
                                           odc::GeneralReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "SetProperties request:\n" << request->DebugString();
    // Convert from protobuf to ODC format
    SSetPropertiesParams::Properties_t props;
    for (int i = 0; i < request->properties_size(); i++)
    {
        auto prop{ request->properties(i) };
        props.push_back(SSetPropertiesParams::Property_t(prop.key(), prop.value()));
    }

    SSetPropertiesParams params{ props, request->path() };
    SReturnValue value{ m_service->execSetProperties(common, params) };
    setupGeneralReply(response, value);
    logResponse("SetProperties response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Configure(::grpc::ServerContext* /*context*/,
                                       const odc::ConfigureRequest* request,
                                       odc::StateReply* response)
{
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Configure request:\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value{ m_service->execConfigure(common, params) };
    setupStateReply(response, value);
    logResponse("Configure response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Start(::grpc::ServerContext* /*context*/,
                                   const odc::StartRequest* request,
                                   odc::StateReply* response)
{
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Start request:\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value{ m_service->execStart(common, params) };
    setupStateReply(response, value);
    logResponse("Start response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Stop(::grpc::ServerContext* /*context*/,
                                  const odc::StopRequest* request,
                                  odc::StateReply* response)
{
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Stop request:\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value{ m_service->execStop(common, params) };
    setupStateReply(response, value);
    logResponse("Stop response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Reset(::grpc::ServerContext* /*context*/,
                                   const odc::ResetRequest* request,
                                   odc::StateReply* response)
{
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Reset request:\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value{ m_service->execReset(common, params) };
    setupStateReply(response, value);
    logResponse("Reset response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Terminate(::grpc::ServerContext* /*context*/,
                                       const odc::TerminateRequest* request,
                                       odc::StateReply* response)
{
    const auto common{ commonParams(&request->request()) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Terminate request:\n" << request->DebugString();
    SDeviceParams params{ request->request().path(), request->request().detailed() };
    SReturnValue value{ m_service->execTerminate(common, params) };
    setupStateReply(response, value);
    logResponse("Terminate response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Shutdown(::grpc::ServerContext* /*context*/,
                                      const odc::ShutdownRequest* request,
                                      odc::GeneralReply* response)
{
    const auto common{ commonParams(request) };
    lock_guard<mutex> lock(getMutex(common.m_partitionID));
    OLOG(ESeverity::info, common) << "Shutdown request:\n" << request->DebugString();
    SReturnValue value{ m_service->execShutdown(common) };
    setupGeneralReply(response, value);
    logResponse("Shutdown response:\n", common, response);
    return ::grpc::Status::OK;
}

::grpc::Status CGrpcService::Status(::grpc::ServerContext* /*context*/,
                                    const odc::StatusRequest* request,
                                    odc::StatusReply* response)
{
    OLOG(ESeverity::info) << "Status request:\n" << request->DebugString();
    SStatusReturnValue value{ m_service->execStatus(SStatusParams(request->running())) };
    setupStatusReply(response, value);
    logResponse("Status response:\n", core::SCommonParams(), response);
    return ::grpc::Status::OK;
}

odc::Error* CGrpcService::newError(const SBaseReturnValue& _value)
{
    odc::Error* error{ new odc::Error() };
    error->set_code(_value.m_error.m_code.value());
    error->set_msg(_value.m_error.m_code.message() + " (" + _value.m_error.m_details + ")");
    return error;
}

void CGrpcService::setupGeneralReply(odc::GeneralReply* _response, const SReturnValue& _value)
{
    if (_value.m_statusCode == EStatusCode::ok)
    {
        _response->set_status(odc::ReplyStatus::SUCCESS);
        _response->set_msg(_value.m_msg);
    }
    else
    {
        _response->set_status(odc::ReplyStatus::ERROR);
        _response->set_allocated_error(newError(_value));
    }
    _response->set_partitionid(_value.m_partitionID);
    _response->set_runnr(_value.m_runNr);
    _response->set_sessionid(_value.m_sessionID);
    _response->set_exectime(_value.m_execTime);
    _response->set_state(GetAggregatedTopologyStateName(_value.m_aggregatedState));
}

void CGrpcService::setupStateReply(odc::StateReply* _response, const odc::core::SReturnValue& _value)
{
    // Protobuf message takes the ownership and deletes the object
    odc::GeneralReply* generalResponse{ new odc::GeneralReply() };
    setupGeneralReply(generalResponse, _value);
    _response->set_allocated_reply(generalResponse);

    if (_value.m_details != nullptr)
    {
        const auto& topologyState = _value.m_details->m_topologyState;
        for (const auto& state : topologyState)
        {
            auto device{ _response->add_devices() };
            device->set_path(state.m_path);
            device->set_id(state.m_status.taskId);
            device->set_state(fair::mq::GetStateName(state.m_status.state));
        }
    }
}

void CGrpcService::setupStatusReply(odc::StatusReply* _response, const odc::core::SStatusReturnValue& _value)
{
    if (_value.m_statusCode == EStatusCode::ok)
    {
        _response->set_status(odc::ReplyStatus::SUCCESS);
        _response->set_msg(_value.m_msg);
    }
    else
    {
        _response->set_status(odc::ReplyStatus::ERROR);
        _response->set_allocated_error(newError(_value));
    }
    _response->set_exectime(_value.m_execTime);
    for (const auto& p : _value.m_partitions)
    {
        auto partition{ _response->add_partitions() };
        partition->set_partitionid(p.m_partitionID);
        partition->set_sessionid(p.m_sessionID);
        partition->set_status(
            (p.m_sessionStatus == ESessionStatus::running ? SessionStatus::RUNNING : SessionStatus::STOPPED));
        partition->set_state(GetAggregatedTopologyStateName(p.m_aggregatedState));
    }
}

std::mutex& CGrpcService::getMutex(const partitionID_t& _partitionID)
{
    std::lock_guard<std::mutex> lock(m_mutexMapMutex);
    auto it{ m_mutexMap.find(_partitionID) };
    return (it == m_mutexMap.end()) ? m_mutexMap[_partitionID] : it->second;
}

template <typename Request_t>
core::SCommonParams CGrpcService::commonParams(const Request_t* _request)
{
    return core::SCommonParams(_request->partitionid(), _request->runnr(), _request->timeout());
}

template <typename Response_t>
void CGrpcService::logResponse(const string& _msg, const core::SCommonParams& _common, const Response_t* _response)
{
    auto severity{ (_response->status() == odc::ReplyStatus::ERROR) ? ESeverity::error : ESeverity::info };
    OLOG(severity, _common) << _msg << _response->DebugString();
}

void CGrpcService::logResponse(const std::string& _msg,
                               const core::SCommonParams& _common,
                               const odc::StateReply* _response)
{
    auto severity{ (_response->reply().status() == odc::ReplyStatus::ERROR) ? ESeverity::error : ESeverity::info };
    OLOG(severity, _common) << _msg << _response->DebugString();
}
