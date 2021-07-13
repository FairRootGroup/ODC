// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcAsyncService__
#define __ODC__GrpcAsyncService__

// ODC
#include "DDSSubmit.h"
#include "GrpcService.h"
#include "Logger.h"
// GRPC
#include <grpcpp/grpcpp.h>

namespace odc::grpc
{
    class CGrpcAsyncService final
    {
      public:
        CGrpcAsyncService();

        void run(const std::string& _host);
        void setTimeout(const std::chrono::seconds& _timeout);
        void registerResourcePlugins(const odc::core::CDDSSubmit::PluginMap_t& _pluginMap);

      private:
        // Class encompasing the state and logic needed to serve a request
        class ICallData
        {
          public:
            virtual void proceed() = 0;
        };

        template <class Request_t, class Reply_t>
        class CCallData : public ICallData
        {

          public:
            /// odc::ODC::AsyncService::Request* method requesting the system to start processing a certain request
            using requestFunc_t = std::function<void(::grpc::ServerContext*,
                                                     Request_t*,
                                                     ::grpc::ServerAsyncResponseWriter<Reply_t>*,
                                                     ::grpc::CompletionQueue*,
                                                     ::grpc::ServerCompletionQueue*,
                                                     void*)>;

            /// odc::CGrpcService method which processes the request
            using processFunc_t = std::function<::grpc::Status(::grpc::ServerContext*, const Request_t*, Reply_t*)>;

          public:
            CCallData(::grpc::ServerCompletionQueue* _cq, requestFunc_t _requestFunc, processFunc_t _processFunc)
                : m_cq(_cq)
                , m_responder(&m_ctx)
                , m_requestFunc(_requestFunc)
                , m_processFunc(_processFunc)
                , m_status(EStatus::create)
            {
                proceed();
            }

            virtual ~CCallData()
            {
            }

            void proceed()
            {
                if (m_status == EStatus::create)
                {
                    m_status = EStatus::process;
                    m_requestFunc(&m_ctx, &m_request, &m_responder, m_cq, m_cq, this);
                }
                else if (m_status == EStatus::process)
                {
                    new CCallData(m_cq, m_requestFunc, m_processFunc);
                    auto status{ m_processFunc(&m_ctx, &m_request, &m_reply) };
                    m_status = EStatus::finish;
                    m_responder.Finish(m_reply, status, this);
                }
                else
                {
                    GPR_ASSERT(m_status == EStatus::finish);
                    delete this;
                }
            }

          private:
            ::grpc::ServerCompletionQueue* m_cq; ///< The producer-consumer queue for asynchronous server notifications
            ::grpc::ServerContext m_ctx;         ///< The context
            Request_t m_request;                 ///< The request
            Reply_t m_reply;                     ///< The reply
            ::grpc::ServerAsyncResponseWriter<Reply_t> m_responder; ///< The means to get back to the client
            requestFunc_t m_requestFunc; ///<  The function requesting the system to start request processing
            processFunc_t m_processFunc; ///< The function doing actual request processing

            // A tiny state machine with the serving states
            enum class EStatus
            {
                create,
                process,
                finish
            };
            EStatus m_status; ///< The current serving state.
        };

        // Actual calls
        using CCallInitialize = CCallData<InitializeRequest, GeneralReply>;
        using CCallSubmit = CCallData<SubmitRequest, GeneralReply>;
        using CCallActivate = CCallData<ActivateRequest, GeneralReply>;
        using CCallRun = CCallData<RunRequest, GeneralReply>;
        using CCallGetState = CCallData<StateRequest, StateReply>;
        using CCallSetProperties = CCallData<SetPropertiesRequest, GeneralReply>;
        using CCallUpdate = CCallData<UpdateRequest, GeneralReply>;
        using CCallConfigure = CCallData<ConfigureRequest, StateReply>;
        using CCallStart = CCallData<StartRequest, StateReply>;
        using CCallStop = CCallData<StopRequest, StateReply>;
        using CCallReset = CCallData<ResetRequest, StateReply>;
        using CCallTerminate = CCallData<TerminateRequest, StateReply>;
        using CCallShutdown = CCallData<ShutdownRequest, GeneralReply>;
        using CCallStatus = CCallData<StatusRequest, StatusReply>;

      private:
        template <class Call_t, class RequestFunc_t, class ProcessFunc_t>
        void make(::grpc::ServerCompletionQueue* _cq,
                  odc::ODC::AsyncService* _service,
                  RequestFunc_t _requestFunc,
                  ProcessFunc_t _processFunc)
        {
            new Call_t(
                _cq,
                std::bind(_requestFunc,
                          _service,
                          std::placeholders::_1,
                          std::placeholders::_2,
                          std::placeholders::_3,
                          std::placeholders::_4,
                          std::placeholders::_5,
                          std::placeholders::_6),
                std::bind(
                    _processFunc, m_service, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        }

        std::shared_ptr<odc::grpc::CGrpcService> m_service; ///< Core gRPC service
    };
} // namespace odc::grpc

#endif /* defined(__ODC__GrpcAsyncService__) */
