// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliControlService.h"
#include "CliHelper.h"
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
    try
    {
        SInitializeParams initializeParams;
        SSubmitParams submitParams;
        SActivateParams activateParams;
        SUpdateParams upscaleParams;
        SUpdateParams downscaleParams;
        CLogger::SConfig logConfig;
        string recoTopoPath;
        string qcTopoPath;

        // Generic options
        bpo::options_description options("odc-cli-server options");
        options.add_options()("help,h", "Produce help message");
        CCliHelper::addInitializeOptions(options, SInitializeParams(1000), initializeParams);
        CCliHelper::addSubmitOptions(options, SSubmitParams("localhost", "", 1, 12), submitParams);
        string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite.xml");
        CCliHelper::addActivateOptions(options, SActivateParams(defaultTopo), activateParams);
        string defaultUpscaleTopo(kODCDataDir + "/ex-dds-topology-infinite-up.xml");
        CCliHelper::addUpscaleOptions(options, SUpdateParams(defaultUpscaleTopo), upscaleParams);
        string defaultDownscaleTopo(kODCDataDir + "/ex-dds-topology-infinite-down.xml");
        CCliHelper::addDownscaleOptions(options, SUpdateParams(defaultDownscaleTopo), downscaleParams);
        CCliHelper::addLogOptions(options, CLogger::SConfig(), logConfig);
        CCliHelper::addPathOptions(options, "", recoTopoPath, "", qcTopoPath);

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
        control.setInitializeParams(initializeParams);
        control.setSubmitParams(submitParams);
        control.setActivateParams(activateParams);
        control.setUpscaleParams(upscaleParams);
        control.setDownscaleParams(downscaleParams);
        control.setRecoTopoPath(recoTopoPath);
        control.setQCTopoPath(qcTopoPath);
        control.run();
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
