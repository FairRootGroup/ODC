// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliHelper.h"
#include "GrpcControlServer.h"
#include "GrpcControlService.h"
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
        size_t timeout;
        string host;
        SSubmitParams submitParams;
        CLogger::SConfig logConfig;

        // Generic options
        bpo::options_description options("dds-control-server options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addVersionOptions(options);
        CCliHelper::addTimeoutOptions(options, timeout);
        CCliHelper::addHostOptions(options, host);
        CCliHelper::addOptions(options, submitParams);
        CCliHelper::addLogOptions(options, logConfig);

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
        server.setTimeout(chrono::seconds(timeout));
        server.setSubmitParams(submitParams);
        server.Run(host);
    }
    catch (exception& _e)
    {
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
