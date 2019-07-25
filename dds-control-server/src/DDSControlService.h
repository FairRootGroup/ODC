// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// STD
#include <string>

// GRPC
#include "ddscontrol.grpc.pb.h"
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
        DDSControlService();

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
        bool submitDDSAgents(size_t _numAgents);
        bool activateDDSTopology(const std::string& _topologyFile);
        bool shutdownDDSSession();
        void setupGeneralReply(ddscontrol::GeneralReply* _response,
                               bool _success,
                               const std::string& _msg,
                               const std::string& _errMsg);
        bool changeState(fair::mq::sdk::TopologyTransition _transition);

      private:
        std::shared_ptr<dds::topology_api::CTopology> m_topo;
        std::shared_ptr<dds::tools_api::CSession> m_session;
        std::shared_ptr<fair::mq::sdk::Topology> m_fairmqTopo;
        const size_t m_timeout; ///< Request timeout in sec
    };
} // namespace ddscontrol
