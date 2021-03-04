// Copyright 2019 GSI, Inc. All rights reserved.
//
//

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
    try
    {
        string res;

        // Generic options
        bpo::options_description options("odc-rp-same options");
        options.add_options()("help,h", "Produce help message");
        options.add_options()("res", bpo::value<string>(&res)->default_value(""), "Resource description");
        // Parsing command-line
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        if (vm.count("help"))
        {
            cout << options;
            return EXIT_SUCCESS;
        }

        cout << res;
    }
    catch (exception& _e)
    {
        cerr << _e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
