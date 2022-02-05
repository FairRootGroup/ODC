// Copyright 2014 GSI, Inc. All rights reserved.
//
// This file contains a number of helpers to calculate execution time of a function.
//
#ifndef __ODC__CCmdsFile__
#define __ODC__CCmdsFile__

#include <string>
#include <vector>

namespace odc::core
{
    class CCmdsFile
    {
      public:
        static std::vector<std::string> getCmds(const std::string& _filepath);
    };
} // namespace odc::core
#endif /*__ODC__CCmdsFile__*/
