/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

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
