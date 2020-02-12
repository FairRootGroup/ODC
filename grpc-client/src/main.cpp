// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliHelper.h"
#include "GrpcControlClient.h"

// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    string host;
    SInitializeParams initializeParams;
    SActivateParams activateParams;
    SUpdateParams upscaleParams;
    SUpdateParams downscaleParams;

    // Generic options
    bpo::options_description options("grpc-client options");
    options.add_options()("help,h", "Produce help message");
    CCliHelper::addHostOptions(options, "localhost:50051", host);
    CCliHelper::addInitializeOptions(options, SInitializeParams(1000), initializeParams);
    string defaultTopo(kBuildFairMQDataDir + "/ex-dds-topology-infinite.xml");
    CCliHelper::addActivateOptions(options, SActivateParams(defaultTopo), activateParams);
    CCliHelper::addUpscaleOptions(options, SUpdateParams(defaultTopo), upscaleParams);
    CCliHelper::addDownscaleOptions(options, SUpdateParams(defaultTopo), downscaleParams);

    // Parsing command-line
    bpo::variables_map vm;
    bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
    bpo::notify(vm);

    if (vm.count("help"))
    {
        cout << options;
        return EXIT_SUCCESS;
    }

    CGrpcControlClient control(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    control.setInitializeParams(initializeParams);
    control.setActivateParams(activateParams);
    control.setUpscaleParams(upscaleParams);
    control.setDownscaleParams(downscaleParams);

    CCliHelper::printDescription();

    while (true)
    {
        string cmd;
        cout << "Please enter command: ";
        getline(std::cin, cmd);
        control.processRequest(cmd);
    }

    return EXIT_SUCCESS;
}
