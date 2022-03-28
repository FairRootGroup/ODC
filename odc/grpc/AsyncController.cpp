/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// DDS
#include <odc/grpc/AsyncController.h>
#include <odc/Logger.h>

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

AsyncController::AsyncController()
    : mCtrl(make_shared<odc::grpc::Controller>())
{}

void AsyncController::setTimeout(const std::chrono::seconds& timeout) { mCtrl->setTimeout(timeout); }

void AsyncController::registerResourcePlugins(const CPluginManager::PluginMap_t& pluginMap) { mCtrl->registerResourcePlugins(pluginMap); }

void AsyncController::registerRequestTriggers(const CPluginManager::PluginMap_t& triggerMap) { mCtrl->registerRequestTriggers(triggerMap); }

void AsyncController::restore(const std::string& restoreId) { mCtrl->restore(restoreId); }

void AsyncController::run(const std::string& host)
{
    odc::ODC::AsyncService service;
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(host, ::grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto cq{ builder.AddCompletionQueue() };
    auto server{ builder.BuildAndStart() };

    // Spawn a new CallData instance for each type of request to serve new clients
    using namespace std::placeholders;
    make<CCallInitialize>   (cq.get(), &service, &odc::ODC::AsyncService::RequestInitialize,    &grpc::Controller::Initialize);
    make<CCallSubmit>       (cq.get(), &service, &odc::ODC::AsyncService::RequestSubmit,        &grpc::Controller::Submit);
    make<CCallActivate>     (cq.get(), &service, &odc::ODC::AsyncService::RequestActivate,      &grpc::Controller::Activate);
    make<CCallRun>          (cq.get(), &service, &odc::ODC::AsyncService::RequestRun,           &grpc::Controller::Run);
    make<CCallGetState>     (cq.get(), &service, &odc::ODC::AsyncService::RequestGetState,      &grpc::Controller::GetState);
    make<CCallSetProperties>(cq.get(), &service, &odc::ODC::AsyncService::RequestSetProperties, &grpc::Controller::SetProperties);
    make<CCallUpdate>       (cq.get(), &service, &odc::ODC::AsyncService::RequestUpdate,        &grpc::Controller::Update);
    make<CCallConfigure>    (cq.get(), &service, &odc::ODC::AsyncService::RequestConfigure,     &grpc::Controller::Configure);
    make<CCallStart>        (cq.get(), &service, &odc::ODC::AsyncService::RequestStart,         &grpc::Controller::Start);
    make<CCallStop>         (cq.get(), &service, &odc::ODC::AsyncService::RequestStop,          &grpc::Controller::Stop);
    make<CCallReset>        (cq.get(), &service, &odc::ODC::AsyncService::RequestReset,         &grpc::Controller::Reset);
    make<CCallTerminate>    (cq.get(), &service, &odc::ODC::AsyncService::RequestTerminate,     &grpc::Controller::Terminate);
    make<CCallShutdown>     (cq.get(), &service, &odc::ODC::AsyncService::RequestShutdown,      &grpc::Controller::Shutdown);
    make<CCallStatus>       (cq.get(), &service, &odc::ODC::AsyncService::RequestStatus,        &grpc::Controller::Status);

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
