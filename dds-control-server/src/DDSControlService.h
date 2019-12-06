// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __DDSControl__DDSControlService__
#define __DDSControl__DDSControlService__

// STD
#include <string>

// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

// DDS
#include <DDS/Tools.h>
#include <DDS/Topology.h>

// FairMQ
#include <fairmq/SDK.h>

namespace ddscontrol
{
    class DDSControlService final : public ddscontrol::DDSControl::Service
    {
      public:
        struct SConfigParams
        {
            std::string m_rmsPlugin = "";
            std::string m_configFile = "";
        };

      public:
        DDSControlService(const SConfigParams& _params);

      private:
        grpc::Status Initialize(grpc::ServerContext* context,
                                const ddscontrol::InitializeRequest* request,
                                ddscontrol::GeneralReply* response) override;
        grpc::Status ConfigureRun(grpc::ServerContext* context,
                                  const ddscontrol::ConfigureRunRequest* request,
                                  ddscontrol::GeneralReply* response) override;
        grpc::Status Start(grpc::ServerContext* context,
                           const ddscontrol::StartRequest* request,
                           ddscontrol::GeneralReply* response) override;
        grpc::Status Stop(grpc::ServerContext* context,
                          const ddscontrol::StopRequest* request,
                          ddscontrol::GeneralReply* response) override;
        grpc::Status Terminate(grpc::ServerContext* context,
                               const ddscontrol::TerminateRequest* request,
                               ddscontrol::GeneralReply* response) override;
        grpc::Status Shutdown(grpc::ServerContext* context,
                              const ddscontrol::ShutdownRequest* request,
                              ddscontrol::GeneralReply* response) override;

      private:
        bool createDDSSession();
        bool submitDDSAgents(size_t _numAgents, size_t _numSlots);
        bool activateDDSTopology(const std::string& _topologyFile);
        bool waitForNumActiveAgents(size_t _numAgents);
        bool shutdownDDSSession();
        void setupGeneralReply(ddscontrol::GeneralReply* _response,
                               bool _success,
                               const std::string& _msg,
                               const std::string& _errMsg,
                               size_t _execTime);
        bool changeState(fair::mq::sdk::TopologyTransition _transition);

      private:
        std::shared_ptr<dds::topology_api::CTopology> m_topo;
        std::shared_ptr<dds::tools_api::CSession> m_session;
        std::shared_ptr<fair::mq::sdk::Topology> m_fairmqTopo;
        const size_t m_timeout; ///< Request timeout in sec
        SConfigParams m_configParams;
    };
} // namespace ddscontrol

#endif /* defined(__DDSControl__DDSControlService__) */
