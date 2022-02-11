/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__GrpcControlClient__
#define __ODC__GrpcControlClient__

// ODC
#include <odc/CliServiceHelper.h>
// STD
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
// GRPC
#include <odc/grpc/odc.grpc.pb.h>
#include <grpcpp/grpcpp.h>

class CGrpcControlClient : public odc::core::CCliServiceHelper<CGrpcControlClient>
{
  public:
    CGrpcControlClient(std::shared_ptr<grpc::Channel> channel);

    std::string requestInitialize(const odc::core::SCommonParams& _common, const odc::core::SInitializeParams& _params);
    std::string requestSubmit(const odc::core::SCommonParams& _common, const odc::core::SSubmitParams& _params);
    std::string requestActivate(const odc::core::SCommonParams& _common, const odc::core::SActivateParams& _params);
    std::string requestRun(const odc::core::SCommonParams& _common,
                           const odc::core::SInitializeParams& _initializeParams,
                           const odc::core::SSubmitParams& _submitParams,
                           const odc::core::SActivateParams& _activateParams);
    std::string requestUpscale(const odc::core::SCommonParams& _common, const odc::core::SUpdateParams& _params);
    std::string requestDownscale(const odc::core::SCommonParams& _common, const odc::core::SUpdateParams& _params);
    std::string requestGetState(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
    std::string requestSetProperties(const odc::core::SCommonParams& _common,
                                     const odc::core::SSetPropertiesParams& _params);
    std::string requestConfigure(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
    std::string requestStart(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
    std::string requestStop(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
    std::string requestReset(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
    std::string requestTerminate(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
    std::string requestShutdown(const odc::core::SCommonParams& _common);
    std::string requestStatus(const odc::core::SStatusParams& _params);

  private:
    std::string updateRequest(const odc::core::SCommonParams& _common, const odc::core::SUpdateParams& _params);
    template <typename Request_t, typename StubFunc_t>
    std::string stateChangeRequest(const odc::core::SCommonParams& _common,
                                   const odc::core::SDeviceParams& _params,
                                   StubFunc_t _stubFunc);

  private:
    template <typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply);

    template <typename Request_t>
    void updateCommonParams(const odc::core::SCommonParams& _common, Request_t* _request);

  private:
    std::unique_ptr<odc::ODC::Stub> m_stub;
};

#endif /* defined(__ODC__GrpcControlClient__) */
