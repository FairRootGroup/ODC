// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
#include "CliHelper.h"
#include "GrpcControlClient.h"
#include "Logger.h"
#include "Version.h"
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
        CLogger::SConfig logConfig;
        vector<string> cmds;
        bool batch;
        vector<partitionID_t> partitionIDs;

        // Generic options
        bpo::options_description options("grpc-client options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addVersionOptions(options);
        CCliHelper::addHostOptions(options, host);
        CCliHelper::addLogOptions(options, logConfig);
        CCliHelper::addBatchOptions(options, cmds, batch, partitionIDs);

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

        CGrpcControlClient control(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
        control.run((batch) ? cmds : vector<string>(), partitionIDs, std::chrono::milliseconds(1000));
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
