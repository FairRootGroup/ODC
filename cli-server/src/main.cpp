// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliControlService.h"
#include "CliHelper.h"
#include "Logger.h"
// STD
#include <chrono>
#include <cstdlib>
#include <iostream>
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
// FairMQ SDK
#include <fairmq/sdk/DDSEnvironment.h>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    try
    {
        size_t timeout;
        CLogger::SConfig logConfig;
        vector<string> cmds;
        bool batch;
        vector<partitionID_t> partitionIDs;

        // Generic options
        bpo::options_description options("odc-cli-server options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addTimeoutOptions(options, timeout);
        CCliHelper::addLogOptions(options, logConfig);
        CCliHelper::addBatchOptions(options, cmds, batch);
        CCliHelper::addPartitionOptions(options, partitionIDs);

        // Parsing command-line
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        try
        {
            CLogger::instance().init(logConfig);
        }
        catch (exception& _e)
        {
            cerr << "Can't initialize log: " << _e.what() << endl;
            return EXIT_FAILURE;
        }

        if (vm.count("help"))
        {
            OLOG(ESeverity::clean) << options;
            return EXIT_SUCCESS;
        }

        {
            // Equivalent to calling source DDS_env.sh
            fair::mq::sdk::DDSEnv env;
        }

        // Prepend FairMQ bin dir to the path
        auto current_path(std::getenv("PATH"));
        string new_path;
        if (current_path != nullptr)
        {
            new_path = kBuildFairMQBinDir + string(":") + string(current_path);
        }
        else
        {
            new_path = kBuildFairMQBinDir;
        }
        setenv("PATH", new_path.c_str(), 1);

        odc::cli::CCliControlService control;
        control.setPartitionIDs(partitionIDs);
        control.setTimeout(chrono::seconds(timeout));
        control.run((batch) ? cmds : vector<string>(), std::chrono::milliseconds(1000));
    }
    catch (exception& _e)
    {
        OLOG(ESeverity::clean) << _e.what();
        OLOG(ESeverity::fatal) << _e.what();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        OLOG(ESeverity::fatal) << "Unexpected Exception occurred.";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
