/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

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
        string outputTopoFilepath;
        size_t numDpl;
        size_t numDd;

        // Generic options
        bpo::options_description options("odc-topo options");
        options.add_options()("help,h", "Produce help message");
        string defaultDPLTopo(odc::core::kODCDataDir + "/ex-dpl-topology.xml");
        options.add_options()(
            "dpl", bpo::value<string>(&dplTopoFilepath)->default_value(defaultDPLTopo), "Path to DPL topology file");
        string defaultDDTopo(odc::core::kODCDataDir + "/ex-dd-topology.xml");
        options.add_options()("dd",
                              bpo::value<string>(&ddTopoFilepath)->default_value(defaultDDTopo),
                              "Path to Data Distribution topology file");
        options.add_options()("output,o",
                              bpo::value<string>(&outputTopoFilepath)->default_value("topology.xml"),
                              "Output topology filepath");
        options.add_options()("ndpl", bpo::value<size_t>(&numDpl)->default_value(10), "Number of DPL collections");
        options.add_options()("ndd", bpo::value<size_t>(&numDd)->default_value(10), "Number of DD tasks");
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
        // New DPL group containing DPL collections
        auto dplGroup{ creator.getMainGroup()->addElement<CTopoGroup>("DPLGroup") };
        // Set required number of DPL collections
        dplGroup->setN(numDpl);
        // Add new EPN collection to EPN group
        auto dplC{ dplGroup->addElement<CTopoCollection>("DPL") };
        // Initialize collection from XML topology file
        dplC->initFromXML(dplTopoFilepath);
        // Add new requirement - one DPL collection per host
        auto dplR{ dplC->addRequirement("DPLRequirement") };
        dplR->setRequirementType(CTopoRequirement::EType::MaxInstancesPerHost);
        dplR->setValue("1");
        // New Data Distribution group containing DD tasks
        auto ddGroup{ creator.getMainGroup()->addElement<CTopoGroup>("DDGroup") };
        // Set required number of Data Distribution tasks
        ddGroup->setN(numDd);
        // Add TfBuilder task to DD group and initialize it from XML topology file
        ddGroup->addElement<CTopoTask>("TfBuilderTask")->initFromXML(ddTopoFilepath);

        // Save topology to the oputput file
        creator.save(outputTopoFilepath);
        cout << "New DDS topology successfully created and saved to a file " << quoted(outputTopoFilepath) << endl;

        // Validate created topology
        // Create a topology from the output file
        CTopology topo(outputTopoFilepath);
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
