// Copyright 2014 GSI, Inc. All rights reserved.
//
// This file contains a number of helpers to calculate execution time of a function.
//

#include "CmdsFile.h"
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
