// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliHelper.h"
#include "GrpcControlClient.h"
#include "Logger.h"
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    try
    {
        string host;
        SInitializeParams initializeParams;
        SActivateParams activateParams;
        SUpdateParams upscaleParams;
        SUpdateParams downscaleParams;
        CLogger::SConfig logConfig;
        SDeviceParams recoDeviceParams;
        SDeviceParams qcDeviceParams;
        SSetPropertiesParams setPropertiesParams;
        SSetPropertiesParams setPropertiesDefaultParams({ { "key1", "value1" }, { "key2", "value2" } }, "");
        vector<string> cmds;
        vector<string> defaultCmds{ ".init",    ".submit", ".activate", ".config",    ".start", ".stop",
                                    ".upscale", ".start",  ".stop",     ".downscale", ".start", ".stop",
                                    ".reset",   ".term",   ".down",     ".quit" };
        bool batch;

        // Generic options
        bpo::options_description options("grpc-client options");
        options.add_options()("help,h", "Produce help message");
        CCliHelper::addHostOptions(options, "localhost:50051", host);
        CCliHelper::addInitializeOptions(options, SInitializeParams(1000, ""), initializeParams);
        string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite.xml");
        CCliHelper::addActivateOptions(options, SActivateParams(defaultTopo), activateParams);
        string defaultUpscaleTopo(kODCDataDir + "/ex-dds-topology-infinite-up.xml");
        CCliHelper::addUpscaleOptions(options, SUpdateParams(defaultUpscaleTopo), upscaleParams);
        string defaultDownscaleTopo(kODCDataDir + "/ex-dds-topology-infinite-down.xml");
        CCliHelper::addDownscaleOptions(options, SUpdateParams(defaultDownscaleTopo), downscaleParams);
        CCliHelper::addLogOptions(options, CLogger::SConfig(), logConfig);
        CCliHelper::addDeviceOptions(options, SDeviceParams(), recoDeviceParams, SDeviceParams(), qcDeviceParams);
        CCliHelper::addSetPropertiesOptions(options, setPropertiesDefaultParams, setPropertiesParams);
        CCliHelper::addBatchOptions(options, defaultCmds, cmds, false, batch);

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

        CCliHelper::parseProperties(vm, setPropertiesDefaultParams, setPropertiesParams);

        CGrpcControlClient control(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
        control.setInitializeParams(initializeParams);
        control.setActivateParams(activateParams);
        control.setUpscaleParams(upscaleParams);
        control.setDownscaleParams(downscaleParams);
        control.setRecoDeviceParams(recoDeviceParams);
        control.setQCDeviceParams(qcDeviceParams);
        control.setSetPropertiesParams(setPropertiesParams);
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
