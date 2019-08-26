// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __DDSControl__DDSControlServer__
#define __DDSControl__DDSControlServer__

// STD
#include <string>

// DDS
#include "DDSControlService.h"

namespace ddscontrol
{
    class DDSControlServer final
    {
      public:
        void Run(const std::string& _host, const DDSControlService::SConfigParams& _params);
    };
} // namespace ddscontrol

#endif /* defined(__DDSControl__DDSControlServer__) */
