// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliHelper.h"
#include "GrpcControlServer.h"
#include "GrpcControlService.h"
#include "Logger.h"
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
    string host;
    SSubmitParams submitParams;
    CLogger::SConfig logConfig;

    // Generic options
    bpo::options_description options("dds-control-server options");
    options.add_options()("help,h", "Produce help message");
    CCliHelper::addHostOptions(options, "localhost:50051", host);
    CCliHelper::addSubmitOptions(options, SSubmitParams("localhost", "", 1, 12), submitParams);
    CCliHelper::addLogOptions(options, CLogger::SConfig(), logConfig);

    // Parsing command-line
    bpo::variables_map vm;
    bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
    bpo::notify(vm);

    CLogger::instance().init(logConfig);

    if (vm.count("help"))
    {
        OLOG(ESeverity::log_stdout_clean) << options;
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

    odc::grpc::CGrpcControlServer server;
    server.Run(host, submitParams);

    return EXIT_SUCCESS;
}
