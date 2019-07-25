// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include <string>

namespace ddscontrol
{
    class DDSControlServer final
    {
      public:
        void Run(const std::string& _host);
    };
} // namespace ddscontrol
