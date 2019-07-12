// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// GRPC
#include "ddscontrol.grpc.pb.h"
#include <grpcpp/grpcpp.h>

// DDS
#include "DDS/Tools.h"
#include "DDS/dds_intercom.h"

class DDSControlService final : public ddscontrol::DDSControl::Service
{
  public:
    DDSControlService()
        : m_DDSService()
        , m_DDSCustomCmd(m_DDSService)
    {
    }

    void RunDDS(const std::string& _sessionID)
    {
        // Best practice is to subscribe on errors first, before doing any other function calls.
        // Otherwise there is a chance to miss some of the error messages from DDS.
        m_DDSService.subscribeOnError([](const dds::intercom_api::EErrorCode _errorCode, const std::string& _errorMsg) {
            std::cout << "Error received: error code: " << _errorCode << ", error message: " << _errorMsg << std::endl;
        });

        // Subscribe on custom commands
        m_DDSCustomCmd.subscribe(
            [this](const std::string& _command, const std::string& _condition, uint64_t _senderId) {
                std::cout << "Received custom command " << _command << " condition: " << _condition
                          << " senderId: " << _senderId << std::endl;

                m_changeStateCondition.notify_all();
            });

        // Subscribe on reply from DDS commander server
        m_DDSCustomCmd.subscribeOnReply(
            [](const std::string& _msg) { std::cout << "Received reply message: " << _msg << std::endl; });

        m_DDSService.start(_sessionID);
    }

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

  private:
    void createDDSSession();
    void submitDDSAgents(size_t _numAgents);
    void activateDDSTopology(const std::string& _topologyFile);
    void shutdownDDSSession();

  private:
    dds::intercom_api::CIntercomService m_DDSService; ///< Intercom service.
    dds::intercom_api::CCustomCmd m_DDSCustomCmd;     ///< Custom commands API. Used for communication with commander.
    std::condition_variable m_changeStateCondition;
    std::mutex m_changeStateMutex;
    dds::tools_api::CSession m_session;
};
