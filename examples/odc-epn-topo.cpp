/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// STD
#include <iomanip>
#include <iostream>
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
// DDS
#include <dds/dds.h>

using namespace std;
using namespace dds::topology_api;
namespace bpo = boost::program_options;

void combineDPLCollections(const vector<string>& _topos, CTopoCollection::Ptr_t _collection, const string& _prependExe)
{
    // Combine all tasks from DPL collections to a single collection
    for (const auto& filepath : _topos) {
        CTopoCollection collection("DPL");
        collection.initFromXML(filepath);
        const auto& elements{ collection.getElements() };

        // Check that tasks with the same name don't exist in the collection
        for (const auto& newTask : elements) {
            for (const auto& task : _collection->getElements()) {
                if (newTask->getName() == task->getName()) {
                    stringstream ss;
                    ss << "Task named " << quoted(newTask->getName()) << " from " << quoted(filepath) << " already exists in the output collection " << quoted(_collection->getName());
                    throw runtime_error(ss.str());
                }
            }
        }

        // Add new tasks
        for (const auto& element : elements) {
            auto task{ _collection->addElement<CTopoTask>(element->getName()) };
            task->initFromXML(filepath);
            task->setExe(_prependExe + task->getExe());
        }
    }
}

int main(int argc, char** argv)
{
    try {
        string ddTopo;
        vector<string> recoTopos;
        string monTopo;
        size_t recoN;
        vector<string> calibTopos;
        string recoWorkerNodes;
        string recoAgentGroup;
        string calibWorkerNodes;
        string calibAgentGroup;
        string outputTopo;
        string prependExe;

        // Generic options
        bpo::options_description options("odc-epn-topo options");
        options.add_options()("help,h",       "Produce help message");
        options.add_options()("dd",           bpo::value<string>(&ddTopo)->default_value(""), "Filepath to XML topology of Data Distribution");
        options.add_options()("reco,r",       bpo::value<vector<string>>(&recoTopos)->multitoken(), "Space separated list of filepaths of reconstruction XML topologies");
        options.add_options()("recown",       bpo::value<string>(&recoWorkerNodes)->default_value(""), "Name of the reco worker node");
        options.add_options()("recogroup",    bpo::value<string>(&recoAgentGroup)->default_value(""), "Name of the reco agent group");
        options.add_options()("mon",          bpo::value<string>(&monTopo)->default_value(""), "Filepath to XML topology of a stderr monitor tool");
        options.add_options()("n",            bpo::value<size_t>(&recoN)->default_value(1), "Multiplicator for the reconstruction group");
        options.add_options()("calib,c",      bpo::value<vector<string>>(&calibTopos)->multitoken(), "Space separated list of filepaths of calibration XML topologies");
        options.add_options()("calibwn",      bpo::value<string>(&calibWorkerNodes)->default_value(""), "Name of the calibration worker node");
        options.add_options()("calibgroup",   bpo::value<string>(&calibAgentGroup)->default_value(""), "Name of the calibration agent group");
        options.add_options()("prependexe,p", bpo::value<string>(&prependExe)->default_value(""), "Prepend with the command all exe tags");
        options.add_options()("output,o",     bpo::value<string>(&outputTopo)->default_value("topology.xml"), "Output topology filepath");
        // Parsing command-line
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            cout << options;
            return EXIT_SUCCESS;
        }

        // DDS topology creator
        CTopoCreator creator;

        // Reconstruction
        if ((!ddTopo.empty() || recoTopos.size() > 0) && recoN > 0) {
            cout << "Creating reconstruction collection..." << endl;
            // New Reco group containing Reco DPL collections
            auto recoGroup{ creator.getMainGroup()->addElement<CTopoGroup>("RecoGroup") };
            // Set required number of Reco DPL collections
            recoGroup->setN(recoN);
            // New Reco DPL collection containing TfBuilder task and all tasks from Reco DPL collections
            auto recoCollection{ recoGroup->addElement<CTopoCollection>("RecoCollection") };
            // Add TfBuilder task and initialize it from XML topology file
            if (!ddTopo.empty()) {
                auto ddTask{ recoCollection->addElement<CTopoTask>("TfBuilderTask") };
                ddTask->initFromXML(ddTopo);
                ddTask->setExe(prependExe + ddTask->getExe());
            }
            // Combine all tasks from reco DPL collections to a reco collection
            combineDPLCollections(recoTopos, recoCollection, prependExe);
            // Add stderr monitor task and initialize it from XML topology file
            if (!monTopo.empty()) {
                auto monTask{ recoCollection->addElement<CTopoTask>("ErrorMonitorTask") };
                monTask->initFromXML(monTopo);
                monTask->setExe(prependExe + monTask->getExe());
            }
            // Add new requirement - one Reco DPL collection per host
            auto recoR{ recoCollection->addRequirement("RecoInstanceRequirement") };
            recoR->setRequirementType(CTopoRequirement::EType::MaxInstancesPerHost);
            recoR->setValue("1");
            if (!recoWorkerNodes.empty()) {
                // Add new worker node name requirement
                auto recoR2{ recoCollection->addRequirement("RecoWnRequirement") };
                recoR2->setRequirementType(CTopoRequirement::EType::WnName);
                recoR2->setValue(recoWorkerNodes);
            } else if (!recoAgentGroup.empty()) {
                // Add new worker node name requirement
                auto recoR2{ recoCollection->addRequirement("RecoAgentGroupRequirement") };
                recoR2->setRequirementType(CTopoRequirement::EType::GroupName);
                recoR2->setValue(recoAgentGroup);
            }
        }

        // Calibration
        if (calibTopos.size() > 0) {
            cout << "Creating calibration collection..." << endl;
            // New calibration DPL collection containing all tasks from calibration DPL collections
            auto calibCollection{ creator.getMainGroup()->addElement<CTopoCollection>("CalibCollection") };
            // Combine all tasks from calobration DPL collections to a calibration collection
            combineDPLCollections(calibTopos, calibCollection, prependExe);
            // Add stderr monitor task and initialize it from XML topology file
            if (!monTopo.empty()) {
                auto monTask{ calibCollection->addElement<CTopoTask>("ErrorMonitorTask") };
                monTask->initFromXML(monTopo);
                monTask->setExe(prependExe + monTask->getExe());
            }
            if (!calibWorkerNodes.empty()) {
                // Add new requirement - calibration worker node name
                auto calibR{ calibCollection->addRequirement("CalibWnRequirement") };
                calibR->setRequirementType(CTopoRequirement::EType::WnName);
                calibR->setValue(calibWorkerNodes);
            } else if (!calibAgentGroup.empty()) {
                // Add new requirement - calibration worker node name
                auto calibR{ calibCollection->addRequirement("CalibAgentGroupRequirement") };
                calibR->setRequirementType(CTopoRequirement::EType::GroupName);
                calibR->setValue(calibAgentGroup);
            }
        }

        // Save topology to the oputput file
        creator.save(outputTopo);
        cout << "New DDS topology successfully created and saved to a file " << quoted(outputTopo) << endl;

        // Validate created topology
        // Create a topology from the output file
        CTopology topo(outputTopo);
        cout << "DDS topology " << quoted(topo.getName()) << " successfully opened from file " << quoted(topo.getFilepath()) << endl;
    } catch (exception& _e) {
        cerr << _e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
