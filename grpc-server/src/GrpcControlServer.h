// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlServer__
#define __ODC__GrpcControlServer__

// STD
#include <string>
// ODC
#include "DDSSubmit.h"
#include "GrpcControlService.h"

namespace odc::grpc
{
    class CGrpcControlServer final
    {
      public:
        CGrpcControlServer();

        void Run(const std::string& _host);

        void setTimeout(const std::chrono::seconds& _timeout);
        void registerResourcePlugins(const odc::core::CDDSSubmit::PluginMap_t& _pluginMap);

      private:
        std::shared_ptr<CGrpcControlService> m_service; ///< Service for request processing
    };
} // namespace odc::grpc

#endif /* defined(__ODC__GrpcControlServer__) */
