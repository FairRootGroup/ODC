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

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace odc::core {

class CCmdsFile
{
  public:
    static std::vector<std::string> getCmds(const std::string& _filepath)
    {
        std::ifstream fs(_filepath, std::ifstream::in);
        if (!fs) {
            std::stringstream ss;
            ss << "Failed to open commands file " << std::quoted(_filepath);
            throw std::runtime_error(ss.str());
        }

        std::vector<std::string> result;

        std::string line;
        while (getline(fs, line)) {
            if (line.length() > 0) {
                result.push_back(line);
            }
        }
        return result;
    }
};

} // namespace odc::core

#endif /*__ODC__CCmdsFile__*/
