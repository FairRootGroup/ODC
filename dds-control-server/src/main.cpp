// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "DDSControlServer.h"

// STD
#include <iostream>

// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    std::string host("");

    // Generic options
    bpo::options_description options("dds-control-server options");
    options.add_options()("help,h", "Produce help message");
    options.add_options()("host", bpo::value<std::string>(&host), "Server address e.g. \"localhost:50051\"");

    // Parsing command-line
    bpo::variables_map vm;
    bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
    bpo::notify(vm);

    if (vm.count("help"))
    {
        cout << options;
        return EXIT_SUCCESS;
    }

    if (!vm.count("host"))
    {
        cout << "DDS control server address is not provided" << endl;
        cout << options;
        return EXIT_FAILURE;
    }

    DDSControlServer server;
    server.Run(host);

    return EXIT_SUCCESS;
}
