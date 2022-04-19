/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ODC
#include <odc/BuildConstants.h>
#include <odc/CliController.h>
#include <odc/CliHelper.h>
#include <odc/Logger.h>
#include <odc/Version.h>
// STD
#include <chrono>
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
    try {
        size_t timeout;
        Logger::Config logConfig;
        CliHelper::BatchOptions batchOptions;
        bool batch;
        CPluginManager::PluginMap_t pluginMap;
        CPluginManager::PluginMap_t triggerMap;
        string restoreId;

        // Generic options
        bpo::options_description options("odc-cli-server options");
        options.add_options()("help,h", "Print help");
        options.add_options()("version,v", "Print version");
        options.add_options()("timeout", boost::program_options::value<size_t>(&timeout)->default_value(30), "Timeout of requests in sec");
        CliHelper::addLogOptions(options, logConfig);
        CliHelper::addBatchOptions(options, batchOptions, batch);
        options.add_options()("rp", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Register resource plugins ( name1:cmd1 name2:cmd2 )");
        options.add_options()("rt", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Register request triggers ( name1:cmd1 name2:cmd2 )");
        options.add_options()("restore", boost::program_options::value<std::string>(&restoreId)->default_value(""), "If set ODC will restore the sessions from file with specified ID");

        // Parsing command-line
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        try {
            Logger::instance().init(logConfig);
        } catch (exception& _e) {
            cerr << "Can't initialize log: " << _e.what() << endl;
            return EXIT_FAILURE;
        }

        if (vm.count("help")) {
            OLOG(clean) << options;
            return EXIT_SUCCESS;
        }

        if (vm.count("version")) {
            OLOG(clean) << ODC_VERSION;
            return EXIT_SUCCESS;
        }

        CliHelper::batchCmds(vm, batch, batchOptions);
        CliHelper::parsePluginMapOptions(vm, pluginMap, "rp");
        CliHelper::parsePluginMapOptions(vm, triggerMap, "rt");

        odc::cli::Controller control;
        control.setTimeout(chrono::seconds(timeout));
        control.registerResourcePlugins(pluginMap);
        control.registerRequestTriggers(triggerMap);
        if (!restoreId.empty()) {
            control.restore(restoreId);
        }
        control.run(batchOptions.mOutputCmds);
    } catch (exception& _e) {
        OLOG(clean) << _e.what();
        OLOG(fatal) << _e.what();
        return EXIT_FAILURE;
    } catch (...) {
        OLOG(fatal) << "Unexpected Exception occurred.";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
