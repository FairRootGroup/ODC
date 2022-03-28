/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// DDS
#include <odc/grpc/GrpcAsyncService.h>
#include <odc/Logger.h>

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

CGrpcAsyncService::CGrpcAsyncService()
    : m_service(make_shared<GrpcController>())
{}

void CGrpcAsyncService::setTimeout(const std::chrono::seconds& _timeout) { m_service->setTimeout(_timeout); }

void CGrpcAsyncService::registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap) { m_service->registerResourcePlugins(_pluginMap); }

void CGrpcAsyncService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap) { m_service->registerRequestTriggers(_triggerMap); }

void CGrpcAsyncService::restore(const std::string& _restoreId) { m_service->restore(_restoreId); }

void CGrpcAsyncService::run(const std::string& _host)
{
    odc::ODC::AsyncService service;
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(_host, ::grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto cq{ builder.AddCompletionQueue() };
    auto server{ builder.BuildAndStart() };

    // Spawn a new CallData instance for each type of request to serve new clients
    using namespace std::placeholders;
    make<CCallInitialize>   (cq.get(), &service, &odc::ODC::AsyncService::RequestInitialize,    &GrpcController::Initialize);
    make<CCallSubmit>       (cq.get(), &service, &odc::ODC::AsyncService::RequestSubmit,        &GrpcController::Submit);
    make<CCallActivate>     (cq.get(), &service, &odc::ODC::AsyncService::RequestActivate,      &GrpcController::Activate);
    make<CCallRun>          (cq.get(), &service, &odc::ODC::AsyncService::RequestRun,           &GrpcController::Run);
    make<CCallGetState>     (cq.get(), &service, &odc::ODC::AsyncService::RequestGetState,      &GrpcController::GetState);
    make<CCallSetProperties>(cq.get(), &service, &odc::ODC::AsyncService::RequestSetProperties, &GrpcController::SetProperties);
    make<CCallUpdate>       (cq.get(), &service, &odc::ODC::AsyncService::RequestUpdate,        &GrpcController::Update);
    make<CCallConfigure>    (cq.get(), &service, &odc::ODC::AsyncService::RequestConfigure,     &GrpcController::Configure);
    make<CCallStart>        (cq.get(), &service, &odc::ODC::AsyncService::RequestStart,         &GrpcController::Start);
    make<CCallStop>         (cq.get(), &service, &odc::ODC::AsyncService::RequestStop,          &GrpcController::Stop);
    make<CCallReset>        (cq.get(), &service, &odc::ODC::AsyncService::RequestReset,         &GrpcController::Reset);
    make<CCallTerminate>    (cq.get(), &service, &odc::ODC::AsyncService::RequestTerminate,     &GrpcController::Terminate);
    make<CCallShutdown>     (cq.get(), &service, &odc::ODC::AsyncService::RequestShutdown,      &GrpcController::Shutdown);
    make<CCallStatus>       (cq.get(), &service, &odc::ODC::AsyncService::RequestStatus,        &GrpcController::Status);

    void* tag;
    bool ok;
    while (true) {
        // Block waiting to read the next event from the completion queue.
        // The event is uniquely identified by its tag, which in this case is the
        // memory address of a CallData instance.
        GPR_ASSERT(cq->Next(&tag, &ok));
        GPR_ASSERT(ok);
        static_cast<ICallData*>(tag)->proceed();
    }
}
