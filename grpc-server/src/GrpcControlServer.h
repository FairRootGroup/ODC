// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlServer__
#define __ODC__GrpcControlServer__

// STD
#include <string>
// ODC
#include "GrpcControlService.h"

namespace odc
{
    namespace grpc
    {
        class CGrpcControlServer final
        {
          public:
            void Run(const std::string& _host, const odc::core::SSubmitParams& _submitParams);
        };
    } // namespace grpc
} // namespace odc

#endif /* defined(__ODC__GrpcControlServer__) */
