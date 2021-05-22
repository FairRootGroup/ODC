// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliHelper.h"
#include "GrpcAsyncService.h"
#include "GrpcSyncService.h"
#include "Logger.h"
#include "MiscUtils.h"
#include "Version.h"
// STD
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
        bool sync;
        size_t timeout;
        string host;
        CLogger::SConfig logConfig;
        CDDSSubmit::PluginMap_t pluginMap;

        // Generic options
        bpo::options_description options("dds-control-server options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addVersionOptions(options);
        CCliHelper::addSyncOptions(options, sync);
        CCliHelper::addTimeoutOptions(options, timeout);
        CCliHelper::addHostOptions(options, host);
        CCliHelper::addLogOptions(options, logConfig);
        CCliHelper::addResourcePluginOptions(options, pluginMap);

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

        if (vm.count("version"))
        {
            OLOG(ESeverity::clean) << ODC_VERSION;
            return EXIT_SUCCESS;
        }

        setupGrpcVerbosity(logConfig);

        CCliHelper::parseResourcePluginOptions(vm, pluginMap);

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

        if (sync)
        {
            odc::grpc::CGrpcSyncService server;
            server.setTimeout(chrono::seconds(timeout));
            server.registerResourcePlugins(pluginMap);
            server.run(host);
        }
        else
        {
            odc::grpc::CGrpcAsyncService server;
            server.setTimeout(chrono::seconds(timeout));
            server.registerResourcePlugins(pluginMap);
            server.run(host);
        }
    }
    catch (exception& _e)
    {
        OLOG(ESeverity::clean) << _e.what();
        OLOG(ESeverity::fatal) << _e.what();
        return EXIT_FAILURE;
    }
    catch (...)
    {
        OLOG(ESeverity::clean) << "Unexpected Exception occurred.";
        OLOG(ESeverity::fatal) << "Unexpected Exception occurred.";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
