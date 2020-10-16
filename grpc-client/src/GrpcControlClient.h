// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlClient__
#define __ODC__GrpcControlClient__

// ODC
#include "CliServiceHelper.h"
// STD
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
// GRPC
#include "odc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class CGrpcControlClient : public odc::core::CCliServiceHelper<CGrpcControlClient>
{
  public:
    CGrpcControlClient(std::shared_ptr<grpc::Channel> channel);

    std::string requestInitialize(const odc::core::partitionID_t& _partitionID,
                                  const odc::core::SInitializeParams& _params);
    std::string requestSubmit(const odc::core::partitionID_t& _partitionID, const odc::core::SSubmitParams& _params);
    std::string requestActivate(const odc::core::partitionID_t& _partitionID,
                                const odc::core::SActivateParams& _params);
    std::string requestRun(const odc::core::partitionID_t& _partitionID,
                           const odc::core::SInitializeParams& _initializeParams,
                           const odc::core::SSubmitParams& _submitParams,
                           const odc::core::SActivateParams& _activateParams);
    std::string requestUpscale(const odc::core::partitionID_t& _partitionID, const odc::core::SUpdateParams& _params);
    std::string requestDownscale(const odc::core::partitionID_t& _partitionID, const odc::core::SUpdateParams& _params);
    std::string requestGetState(const odc::core::partitionID_t& _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestSetProperties(const odc::core::partitionID_t& _partitionID,
                                     const odc::core::SSetPropertiesParams& _params);
    std::string requestConfigure(const odc::core::partitionID_t& _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestStart(const odc::core::partitionID_t& _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestStop(const odc::core::partitionID_t& _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestReset(const odc::core::partitionID_t& _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestTerminate(const odc::core::partitionID_t& _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestShutdown(const odc::core::partitionID_t& _partitionID);

  private:
    std::string updateRequest(const odc::core::partitionID_t& _partitionID, const odc::core::SUpdateParams& _params);
    template <typename Request_t, typename StubFunc_t>
    std::string stateChangeRequest(const odc::core::partitionID_t& _partitionID,
                                   const odc::core::SDeviceParams& _params,
                                   StubFunc_t _stubFunc);

  private:
    template <typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply);

  private:
    std::unique_ptr<odc::ODC::Stub> m_stub;
};

#endif /* defined(__ODC__GrpcControlClient__) */
