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
#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/Version.h>
#include <odc/grpc/GrpcControlClient.h>
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    try {
        string host;
        CLogger::SConfig logConfig;
        CCliHelper::SBatchOptions bopt;
        bool batch;

        // Generic options
        bpo::options_description options("grpc-client options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addVersionOptions(options);
        CCliHelper::addHostOptions(options, host);
        CCliHelper::addLogOptions(options, logConfig);
        CCliHelper::addBatchOptions(options, bopt, batch);

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
        setupGrpcVerbosity(logConfig.m_severity);

        CGrpcControlClient control(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
        control.run(bopt.m_outputCmds);
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
