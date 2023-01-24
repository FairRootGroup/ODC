/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <odc/BuildConstants.h>
#include <odc/CliController.h>
#include <odc/CliHelper.h>
#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/Version.h>

#include <dds/Tools.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>

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
        PluginManager::PluginMap plugins;
        PluginManager::PluginMap triggers;
        string restoreId;
        string restoreDir;
        string historyDir;

        bpo::options_description options("odc-cli-server options");
        options.add_options()
            ("help,h", "Print help")
            ("version,v", "Print version")
            ("timeout", boost::program_options::value<size_t>(&timeout)->default_value(30), "Timeout of requests in sec")
            ("rp", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Register resource plugins ( name1:cmd1 name2:cmd2 )")
            ("rt", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Register request triggers ( name1:cmd1 name2:cmd2 )")
            ("restore", boost::program_options::value<std::string>(&restoreId)->default_value(""), "If set ODC will restore the sessions from file with specified ID")
            ("restore-dir", boost::program_options::value<std::string>(&restoreDir)->default_value(smart_path(toString("$HOME/.ODC/restore/"))), "Directory where restore files are kept")
            ("history-dir", boost::program_options::value<std::string>(&historyDir)->default_value(smart_path(toString("$HOME/.ODC/history/"))), "Directory where history file (timestamp, partitionId, sessionId) is kept");
        CliHelper::addLogOptions(options, logConfig);
        CliHelper::addBatchOptions(options, batchOptions, batch);

        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            std::cout << options << std::endl;
            return EXIT_SUCCESS;
        }

        if (vm.count("version")) {
            std::cout << ODC_VERSION << std::endl;
            return EXIT_SUCCESS;
        }

        try {
            Logger::instance().init(logConfig);
        } catch (exception& _e) {
            cerr << "Can't initialize log: " << _e.what() << endl;
            return EXIT_FAILURE;
        }

        CliHelper::batchCmds(vm, batch, batchOptions);
        CliHelper::parsePluginMapOptions(vm, plugins, "rp");
        CliHelper::parsePluginMapOptions(vm, triggers, "rt");

        odc::CliController controller;
        controller.setTimeout(chrono::seconds(timeout));
        controller.setHistoryDir(historyDir);
        controller.registerResourcePlugins(plugins);
        controller.registerRequestTriggers(triggers);
        if (!restoreId.empty()) {
            controller.restore(restoreId, restoreDir);
        }
        controller.run(batchOptions.mOutputCmds);
    } catch (exception& e) {
        std::cout << "Unhandled exception: " << e.what() << std::endl;
        OLOG(fatal) << "Unhandled exception: " << e.what();
        return EXIT_FAILURE;
    } catch (...) {
        std::cout << "Unexpected unhandled exception occurred." << std::endl;
        OLOG(fatal) << "Unexpected unhandled exception occurred.";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
