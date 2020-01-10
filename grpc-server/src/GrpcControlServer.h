// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlServer__
#define __ODC__GrpcControlServer__

// STD
#include <string>

// DDS
#include "GrpcControlService.h"

namespace odc
{
    namespace grpc
    {
        class CGrpcControlServer final
        {
          public:
            void Run(const std::string& _host, const std::string& _rmsPlugin, const std::string& _configFile);
        };
    } // namespace grpc
} // namespace odc

#endif /* defined(__ODC__GrpcControlServer__) */
