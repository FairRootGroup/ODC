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

    std::string requestInitialize(odc::core::partitionID_t _partitionID, const odc::core::SInitializeParams& _params);
    std::string requestSubmit(odc::core::partitionID_t _partitionID, const odc::core::SSubmitParams& _params);
    std::string requestActivate(odc::core::partitionID_t _partitionID, const odc::core::SActivateParams& _params);
    std::string requestRun(odc::core::partitionID_t _partitionID,
                           const odc::core::SInitializeParams& _initializeParams,
                           const odc::core::SSubmitParams& _submitParams,
                           const odc::core::SActivateParams& _activateParams);
    std::string requestUpscale(odc::core::partitionID_t _partitionID, const odc::core::SUpdateParams& _params);
    std::string requestDownscale(odc::core::partitionID_t _partitionID, const odc::core::SUpdateParams& _params);
    std::string requestGetState(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestSetProperties(odc::core::partitionID_t _partitionID,
                                     const odc::core::SSetPropertiesParams& _params);
    std::string requestConfigure(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestStart(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestStop(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestReset(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestTerminate(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
    std::string requestShutdown(odc::core::partitionID_t _partitionID);

  private:
    std::string updateRequest(odc::core::partitionID_t _partitionID, const odc::core::SUpdateParams& _params);
    template <typename Request_t, typename StubFunc_t>
    std::string stateChangeRequest(odc::core::partitionID_t _partitionID,
                                   const odc::core::SDeviceParams& _params,
                                   StubFunc_t _stubFunc);

  private:
    template <typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply);

  private:
    std::unique_ptr<odc::ODC::Stub> m_stub;
};

#endif /* defined(__ODC__GrpcControlClient__) */
