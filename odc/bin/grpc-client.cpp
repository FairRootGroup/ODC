/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ODC
#include <odc/BuildConstants.h>
#include <odc/CliHelper.h>
#include <odc/MiscUtils.h>
#include <odc/Version.h>
#include <odc/grpc/ControlClient.h>
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <iostream>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    try {
        string host;
        ESeverity gRPCSeverity = ESeverity::info;
        CliHelper::BatchOptions batchOptions;
        bool batch;

        bpo::options_description options("grpc-client options");
        options.add_options()("severity", bpo::value<ESeverity>(&gRPCSeverity)->default_value(ESeverity::info), "gRPC verbosity (inf/dbg/err)");
        CliHelper::addHelpOptions(options);
        CliHelper::addVersionOptions(options);
        CliHelper::addHostOptions(options, host);
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

        CliHelper::batchCmds(vm, batch, batchOptions);
        setupGrpcVerbosity(gRPCSeverity);

        GrpcControlClient control(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
        control.run(batchOptions.mOutputCmds);
    } catch (exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cout << "Unexpected Exception occurred." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
