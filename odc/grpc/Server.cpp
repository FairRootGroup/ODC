/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/


#include <odc/BuildConstants.h>
#include <odc/CliHelper.h>
#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/Version.h>
#include <odc/grpc/AsyncController.h>
#include <odc/grpc/SyncController.h>

#include <dds/Tools.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <cstdlib>
#include <iostream>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

template<typename C>
void runController(C& ctrl, size_t timeout, CPluginManager::PluginMap_t& plugins, CPluginManager::PluginMap_t& triggers, const std::string& restoreId, const std::string& host)
{
    ctrl.setTimeout(chrono::seconds(timeout));
    ctrl.registerResourcePlugins(plugins);
    ctrl.registerRequestTriggers(triggers);
    if (!restoreId.empty()) {
        ctrl.restore(restoreId);
    }
    ctrl.run(host);
}

int main(int argc, char** argv)
{
    try {
        bool sync;
        size_t timeout;
        string host;
        Logger::Config logConfig;
        CPluginManager::PluginMap_t plugins;
        CPluginManager::PluginMap_t triggers;
        string restoreId;

        bpo::options_description options("dds-control-server options");
        options.add_options()("help,h", "Print help");
        options.add_options()("version,v", "Print version");
        options.add_options()("sync", boost::program_options::bool_switch(&sync)->default_value(false), "Use sync implementation of the gRPC server");
        options.add_options()("timeout", boost::program_options::value<size_t>(&timeout)->default_value(30), "Timeout of requests in sec");
        options.add_options()("host", boost::program_options::value<std::string>(&host)->default_value("localhost:50051"), "Server address");
        CliHelper::addLogOptions(options, logConfig);
        options.add_options()("rp", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Register resource plugins ( name1:cmd1 name2:cmd2 )");
        options.add_options()("rt", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Register request triggers ( name1:cmd1 name2:cmd2 )");
        options.add_options()("restore", boost::program_options::value<std::string>(&restoreId)->default_value(""), "If set ODC will restore the sessions from file with specified ID");

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

        CliHelper::parsePluginMapOptions(vm, plugins, "rp");
        CliHelper::parsePluginMapOptions(vm, triggers, "rt");

        if (sync) {
            odc::grpc::SyncController ctrl;
            runController(ctrl, timeout, plugins, triggers, restoreId, host);
        } else {
            odc::grpc::AsyncController ctrl;
            runController(ctrl, timeout, plugins, triggers, restoreId, host);
        }
    } catch (exception& _e) {
        OLOG(clean) << _e.what();
        OLOG(fatal) << _e.what();
        return EXIT_FAILURE;
    } catch (...) {
        OLOG(clean) << "Unexpected Exception occurred.";
        OLOG(fatal) << "Unexpected Exception occurred.";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
