/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_GRPC_ASYNCCONTROLLER
#define ODC_GRPC_ASYNCCONTROLLER

// ODC
#include <odc/DDSSubmit.h>
#include <odc/grpc/Controller.h>
#include <odc/Logger.h>
// GRPC
#include <grpcpp/grpcpp.h>

namespace odc::grpc {

class AsyncController final
{
  public:
    AsyncController() {}

    void run(const std::string& host)
    {
        odc::ODC::AsyncService service;
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(host, ::grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        auto cq{ builder.AddCompletionQueue() };
        auto server{ builder.BuildAndStart() };

        // Spawn a new CallData instance for each type of request to serve new clients
        using namespace std::placeholders;
        make<CallInitialize>   (cq.get(), &service, &odc::ODC::AsyncService::RequestInitialize,    &grpc::Controller::Initialize);
        make<CallSubmit>       (cq.get(), &service, &odc::ODC::AsyncService::RequestSubmit,        &grpc::Controller::Submit);
        make<CallActivate>     (cq.get(), &service, &odc::ODC::AsyncService::RequestActivate,      &grpc::Controller::Activate);
        make<CallRun>          (cq.get(), &service, &odc::ODC::AsyncService::RequestRun,           &grpc::Controller::Run);
        make<CallGetState>     (cq.get(), &service, &odc::ODC::AsyncService::RequestGetState,      &grpc::Controller::GetState);
        make<CallSetProperties>(cq.get(), &service, &odc::ODC::AsyncService::RequestSetProperties, &grpc::Controller::SetProperties);
        make<CallUpdate>       (cq.get(), &service, &odc::ODC::AsyncService::RequestUpdate,        &grpc::Controller::Update);
        make<CallConfigure>    (cq.get(), &service, &odc::ODC::AsyncService::RequestConfigure,     &grpc::Controller::Configure);
        make<CallStart>        (cq.get(), &service, &odc::ODC::AsyncService::RequestStart,         &grpc::Controller::Start);
        make<CallStop>         (cq.get(), &service, &odc::ODC::AsyncService::RequestStop,          &grpc::Controller::Stop);
        make<CallReset>        (cq.get(), &service, &odc::ODC::AsyncService::RequestReset,         &grpc::Controller::Reset);
        make<CallTerminate>    (cq.get(), &service, &odc::ODC::AsyncService::RequestTerminate,     &grpc::Controller::Terminate);
        make<CallShutdown>     (cq.get(), &service, &odc::ODC::AsyncService::RequestShutdown,      &grpc::Controller::Shutdown);
        make<CallStatus>       (cq.get(), &service, &odc::ODC::AsyncService::RequestStatus,        &grpc::Controller::Status);

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
    void setTimeout(const std::chrono::seconds& timeout) { mCtrl.setTimeout(timeout); }
    void registerResourcePlugins(const odc::core::PluginManager::PluginMap_t& pluginMap) { mCtrl.registerResourcePlugins(pluginMap); }
    void registerRequestTriggers(const odc::core::PluginManager::PluginMap_t& triggerMap) { mCtrl.registerRequestTriggers(triggerMap); }
    void restore(const std::string& restoreId) { mCtrl.restore(restoreId); }

  private:
    // Class encompasing the state and logic needed to serve a request
    class ICallData
    {
      public:
        virtual void proceed() = 0;
    };

    template<class Request, class Reply>
    class CallData : public ICallData
    {

      public:
        /// odc::ODC::AsyncService::Request* method requesting the system to start processing a certain request
        using requestFunc_t =
            std::function<void(::grpc::ServerContext*, Request*, ::grpc::ServerAsyncResponseWriter<Reply>*, ::grpc::CompletionQueue*, ::grpc::ServerCompletionQueue*, void*)>;

        /// odc::GrpcController method which processes the request
        using processFunc_t = std::function<::grpc::Status(::grpc::ServerContext*, const Request*, Reply*)>;

      public:
        CallData(::grpc::ServerCompletionQueue* cq, requestFunc_t requestFunc, processFunc_t processFunc)
            : mCompletionQueue(cq)
            , mResponder(&mCtx)
            , mRequestFunc(requestFunc)
            , mProcessFunc(processFunc)
            , mStatus(EStatus::create)
        {
            proceed();
        }

        virtual ~CallData() {}

        void proceed()
        {
            if (mStatus == EStatus::create) {
                mStatus = EStatus::process;
                mRequestFunc(&mCtx, &mRequest, &mResponder, mCompletionQueue, mCompletionQueue, this);
            } else if (mStatus == EStatus::process) {
                new CallData(mCompletionQueue, mRequestFunc, mProcessFunc);
                auto status{ mProcessFunc(&mCtx, &mRequest, &mReply) };
                mStatus = EStatus::finish;
                mResponder.Finish(mReply, status, this);
            } else {
                GPR_ASSERT(mStatus == EStatus::finish);
                delete this;
            }
        }

      private:
        ::grpc::ServerCompletionQueue* mCompletionQueue;       ///< The producer-consumer queue for asynchronous server notifications
        ::grpc::ServerContext mCtx;                            ///< The context
        Request mRequest;                                    ///< The request
        Reply mReply;                                        ///< The reply
        ::grpc::ServerAsyncResponseWriter<Reply> mResponder; ///< The means to get back to the client
        requestFunc_t mRequestFunc;                            ///<  The function requesting the system to start request processing
        processFunc_t mProcessFunc;                            ///< The function doing actual request processing

        // A tiny state machine with the serving states
        enum class EStatus
        {
            create,
            process,
            finish
        };
        EStatus mStatus; ///< The current serving state.
    };

    // Actual calls
    using CallInitialize =    CallData<InitializeRequest,    GeneralReply>;
    using CallSubmit =        CallData<SubmitRequest,        GeneralReply>;
    using CallActivate =      CallData<ActivateRequest,      GeneralReply>;
    using CallRun =           CallData<RunRequest,           GeneralReply>;
    using CallGetState =      CallData<StateRequest,         StateReply>;
    using CallSetProperties = CallData<SetPropertiesRequest, GeneralReply>;
    using CallUpdate =        CallData<UpdateRequest,        GeneralReply>;
    using CallConfigure =     CallData<ConfigureRequest,     StateReply>;
    using CallStart =         CallData<StartRequest,         StateReply>;
    using CallStop =          CallData<StopRequest,          StateReply>;
    using CallReset =         CallData<ResetRequest,         StateReply>;
    using CallTerminate =     CallData<TerminateRequest,     StateReply>;
    using CallShutdown =      CallData<ShutdownRequest,      GeneralReply>;
    using CallStatus =        CallData<StatusRequest,        StatusReply>;

  private:
    template<class Call, class RequestFunc, class ProcessFunc>
    void make(::grpc::ServerCompletionQueue* cq, odc::ODC::AsyncService* service, RequestFunc requestFunc, ProcessFunc processFunc)
    {
        new Call(cq,
            std::bind(requestFunc, service, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6),
            std::bind(processFunc, &mCtrl, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );
    }

    odc::grpc::Controller mCtrl; ///< Core gRPC service
};

} // namespace odc::grpc

#endif /* defined(ODC_GRPC_ASYNCCONTROLLER) */
