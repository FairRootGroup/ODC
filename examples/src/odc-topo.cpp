// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "BuildConstants.h"
// STD
#include <iomanip>
#include <iostream>
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
// DDS
#include "dds/dds.h"

using namespace std;
using namespace dds::topology_api;
namespace bpo = boost::program_options;

int main(int argc, char** argv)
{
    try
    {
        string dplTopoFilepath;
        string ddTopoFilepath;
        size_t numCollections;

        // Generic options
        bpo::options_description options("odc-cli-server options");
        options.add_options()("help,h", "Produce help message");
        string defaultDPLTopo(odc::core::kODCDataDir + "/ex-dpl-topology.xml");
        options.add_options()(
            "dpl", bpo::value<string>(&dplTopoFilepath)->default_value(defaultDPLTopo), "Path to DPL topology file");
        string defaultDDTopo(odc::core::kODCDataDir + "/ex-dd-topology.xml");
        options.add_options()("dd",
                              bpo::value<string>(&ddTopoFilepath)->default_value(defaultDDTopo),
                              "Path to Data Distribution topology file");
        options.add_options()("n", bpo::value<size_t>(&numCollections)->default_value(10), "Number of DPL collections");
        // Parsing command-line
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        if (vm.count("help"))
        {
            cout << options;
            return EXIT_SUCCESS;
        }

        // DDS topology creator
        CTopoCreator creator;
        // New EPN group containing DPL collections and Data Distribution tasks
        auto epnGroup{ creator.getMainGroup()->addElement<CTopoGroup>("EPNGroup") };
        // Set required number of DPL collections (number of EPNs)
        epnGroup->setN(numCollections);
        // Add new EPN collection to EPN group and initialize it from XML topology file
        epnGroup->addElement<CTopoCollection>("DPL")->initFromXML(dplTopoFilepath);
        // Add TfBuilder task to EPN group and initialize it from XML topology file
        epnGroup->addElement<CTopoTask>("TfBuilderTask")->initFromXML(ddTopoFilepath);
        // Save topology to the oputput file
        string outputTopoFile{ "ex-epn-topology.xml" };
        creator.save(outputTopoFile);
        cout << "New DDS topology successfully created and saved to a file " << quoted(outputTopoFile) << endl;

        // Validate created topology
        // Create a topology from the output file
        CTopology topo(outputTopoFile);
        cout << "DDS topology " << quoted(topo.getName()) << " successfully opened from file "
             << quoted(topo.getFilepath()) << endl;
    }
    catch (exception& _e)
    {
        cerr << _e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
