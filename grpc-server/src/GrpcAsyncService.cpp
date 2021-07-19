// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "GrpcAsyncService.h"
#include "Logger.h"

using namespace odc;
using namespace odc::core;
using namespace odc::grpc;
using namespace std;

CGrpcAsyncService::CGrpcAsyncService()
    : m_service(make_shared<CGrpcService>())
{
}

void CGrpcAsyncService::setTimeout(const std::chrono::seconds& _timeout)
{
    m_service->setTimeout(_timeout);
}

void CGrpcAsyncService::registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap)
{
    m_service->registerResourcePlugins(_pluginMap);
}

void CGrpcAsyncService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap)
{
    m_service->registerRequestTriggers(_triggerMap);
}

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
    using AS = odc::ODC::AsyncService;
    using GS = CGrpcService;
    make<CCallInitialize>(cq.get(), &service, &AS::RequestInitialize, &GS::Initialize);
    make<CCallSubmit>(cq.get(), &service, &AS::RequestSubmit, &GS::Submit);
    make<CCallActivate>(cq.get(), &service, &AS::RequestActivate, &GS::Activate);
    make<CCallRun>(cq.get(), &service, &AS::RequestRun, &GS::Run);
    make<CCallGetState>(cq.get(), &service, &AS::RequestGetState, &GS::GetState);
    make<CCallSetProperties>(cq.get(), &service, &AS::RequestSetProperties, &GS::SetProperties);
    make<CCallUpdate>(cq.get(), &service, &AS::RequestUpdate, &GS::Update);
    make<CCallConfigure>(cq.get(), &service, &AS::RequestConfigure, &GS::Configure);
    make<CCallStart>(cq.get(), &service, &AS::RequestStart, &GS::Start);
    make<CCallStop>(cq.get(), &service, &AS::RequestStop, &GS::Stop);
    make<CCallReset>(cq.get(), &service, &AS::RequestReset, &GS::Reset);
    make<CCallTerminate>(cq.get(), &service, &AS::RequestTerminate, &GS::Terminate);
    make<CCallShutdown>(cq.get(), &service, &AS::RequestShutdown, &GS::Shutdown);
    make<CCallStatus>(cq.get(), &service, &AS::RequestStatus, &GS::Status);

    void* tag;
    bool ok;
    while (true)
    {
        // Block waiting to read the next event from the completion queue.
        // The event is uniquely identified by its tag, which in this case is the
        // memory address of a CallData instance.
        GPR_ASSERT(cq->Next(&tag, &ok));
        GPR_ASSERT(ok);
        static_cast<ICallData*>(tag)->proceed();
    }
}
