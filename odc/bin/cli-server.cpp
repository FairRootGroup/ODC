/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ODC
#include <odc/BuildConstants.h>
#include <odc/CliControlService.h>
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
        CLogger::SConfig logConfig;
        CCliHelper::SBatchOptions bopt;
        bool batch;
        CPluginManager::PluginMap_t pluginMap;
        CPluginManager::PluginMap_t triggerMap;
        string restoreId;

        // Generic options
        bpo::options_description options("odc-cli-server options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addVersionOptions(options);
        CCliHelper::addTimeoutOptions(options, timeout);
        CCliHelper::addLogOptions(options, logConfig);
        CCliHelper::addBatchOptions(options, bopt, batch);
        CCliHelper::addResourcePluginOptions(options, pluginMap);
        CCliHelper::addRequestTriggersOptions(options, triggerMap);
        CCliHelper::addRestoreOptions(options, restoreId);

        // Parsing command-line
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        try {
            CLogger::instance().init(logConfig);
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

        CCliHelper::batchCmds(vm, batch, bopt);
        CCliHelper::parsePluginMapOptions(vm, pluginMap, "rp");
        CCliHelper::parsePluginMapOptions(vm, triggerMap, "rt");

        odc::cli::CCliControlService control;
        control.setTimeout(chrono::seconds(timeout));
        control.registerResourcePlugins(pluginMap);
        control.registerRequestTriggers(triggerMap);
        if (!restoreId.empty()) {
            control.restore(restoreId);
        }
        control.run(bopt.m_outputCmds);
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
