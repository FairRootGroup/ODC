// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlServer__
#define __ODC__GrpcControlServer__

// STD
#include <string>
// ODC
#include "GrpcControlService.h"

namespace odc::grpc
{
    class CGrpcControlServer final
    {
      public:
        CGrpcControlServer();

        void Run(const std::string& _host);

        void setTimeout(const std::chrono::seconds& _timeout);
        void setSubmitParams(const odc::core::SSubmitParams& _params);

      private:
        std::shared_ptr<CGrpcControlService> m_service; ///< Service for request processing
    };
} // namespace odc::grpc

#endif /* defined(__ODC__GrpcControlServer__) */
