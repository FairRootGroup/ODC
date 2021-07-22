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
// DDS
#include <dds/Tools.h>

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
        CPluginManager::PluginMap_t pluginMap;
        CPluginManager::PluginMap_t triggerMap;

        // Generic options
        bpo::options_description options("dds-control-server options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addVersionOptions(options);
        CCliHelper::addSyncOptions(options, sync);
        CCliHelper::addTimeoutOptions(options, timeout);
        CCliHelper::addHostOptions(options, host);
        CCliHelper::addLogOptions(options, logConfig);
        CCliHelper::addResourcePluginOptions(options, pluginMap);
        CCliHelper::addRequestTriggersOptions(options, triggerMap);

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

        CCliHelper::parsePluginMapOptions(vm, pluginMap, "rp");
        CCliHelper::parsePluginMapOptions(vm, triggerMap, "rt");

        if (sync)
        {
            odc::grpc::CGrpcSyncService server;
            server.setTimeout(chrono::seconds(timeout));
            server.registerResourcePlugins(pluginMap);
            server.registerRequestTriggers(triggerMap);
            server.run(host);
        }
        else
        {
            odc::grpc::CGrpcAsyncService server;
            server.setTimeout(chrono::seconds(timeout));
            server.registerResourcePlugins(pluginMap);
            server.registerRequestTriggers(triggerMap);
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
