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

#include <odc/CmdsFile.h>
// STD
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;
using namespace odc::core;

vector<string> CCmdsFile::getCmds(const string& _filepath)
{
    ifstream fs(_filepath, ifstream::in);
    if (!fs)
    {
        stringstream ss;
        ss << "Failed to open commands file " << quoted(_filepath);
        throw runtime_error(ss.str());
    }

    vector<string> result;

    string line;
    while (getline(fs, line))
    {
        if (line.length() > 0)
        {
            result.push_back(line);
        }
    }
    return result;
}
