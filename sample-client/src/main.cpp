// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "DDSControlClient.h"

// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
namespace bpo = boost::program_options;

void printDescription()
{
    cout << "Sample client for DDS control." << endl
         << "Available commands:" << endl
         << ".quit - Quit the program." << endl
         << ".init - Initialization request." << endl
         << ".config - Configure run request." << endl
         << ".start - Start request." << endl
         << ".stop - Stop request." << endl
         << ".term - Terminate request." << endl
         << ".down - Shutdown request." << endl;
}

int main(int argc, char** argv)
{
    string host;
    string topo;

    // Generic options
    bpo::options_description options("dds-sample-client options");
    options.add_options()("help,h", "Produce help message");
    options.add_options()(
        "host", bpo::value<std::string>(&host), "DDS control connection string, e.g. \"localhost:50051\"");
    options.add_options()("topo", bpo::value<std::string>(&topo), "Topology filepath");

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

    if (!vm.count("topo"))
    {
        cout << "Topology filepath is not provided" << endl;
        cout << options;
        return EXIT_FAILURE;
    }

    DDSControlClient control(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    control.setTopo(topo);

    printDescription();

    while (true)
    {
        string cmd;
        cout << "Please enter command: ";
        getline(std::cin, cmd);

        string replyString;

        if (cmd.empty())
        {
        }
        else if (cmd == ".quit")
        {
            return EXIT_SUCCESS;
        }
        else if (cmd == ".init")
        {
            cout << "Sending initialization request..." << endl;
            replyString = control.RequestInitialize();
        }
        else if (cmd == ".config")
        {
            cout << "Sending configure run request..." << endl;
            replyString = control.RequestConfigureRun();
        }
        else if (cmd == ".start")
        {
            cout << "Sending start request..." << endl;
            replyString = control.RequestStart();
        }
        else if (cmd == ".stop")
        {
            cout << "Sending stop request..." << endl;
            replyString = control.RequestStop();
        }
        else if (cmd == ".term")
        {
            cout << "Sending terminate request..." << endl;
            replyString = control.RequestTerminate();
        }
        else if (cmd == ".down")
        {
            cout << "Sending shutdown request..." << endl;
            replyString = control.RequestShutdown();
        }
        else
        {
            cout << "Unknown command " << cmd << endl;
        }

        if (!replyString.empty())
        {
            cout << "Reply: " << replyString << endl;
        }
    }

    return EXIT_SUCCESS;
}
