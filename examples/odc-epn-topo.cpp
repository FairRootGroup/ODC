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
#include <dds/TopoVars.h>

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

string removeExtension(const std::string& str)
{
    return str.substr(0, str.find_last_of("."));
}

int main(int argc, char** argv)
{
    try {
        string ddTopo;
        vector<string> recoTopos;
        string monitorTask;
        size_t recoN;
        size_t recoNmin;
        vector<string> calibTopos;
        string recoWorkerNodes;
        string recoAgentGroup;
        string calibWorkerNodes;
        string calibAgentGroup;
        string outputTopo;
        string prependExe;

        bpo::options_description options("odc-epn-topo options");
        options.add_options()
            ("help,h",       "Produce help message")
            ("dd",           bpo::value<string>(&ddTopo)->default_value(""),                 "Filepath to XML topology of Data Distribution")
            ("reco,r",       bpo::value<vector<string>>(&recoTopos)->multitoken(),           "Space separated list of filepaths of reconstruction XML topologies")
            ("recown",       bpo::value<string>(&recoWorkerNodes)->default_value(""),        "Name of the reconstruction worker node")
            ("recogroup",    bpo::value<string>(&recoAgentGroup)->default_value(""),         "Name of the reconstruction agent group")
            ("mon",          bpo::value<string>(&monitorTask)->default_value(""),            "Filepath to XML topology of a stderr monitor tool")
            ("n",            bpo::value<size_t>(&recoN)->default_value(1),                   "Multiplicator for the reconstruction group")
            ("nmin",         bpo::value<size_t>(&recoNmin)->default_value(0),                "Minimum number of working reco groups before failing a run")
            ("calib,c",      bpo::value<vector<string>>(&calibTopos)->multitoken(),          "Space separated list of filepaths of calibration XML topologies")
            ("calibwn",      bpo::value<string>(&calibWorkerNodes)->default_value(""),       "Name of the calibration worker node")
            ("calibgroup",   bpo::value<string>(&calibAgentGroup)->default_value(""),        "Name of the calibration agent group")
            ("prependexe,p", bpo::value<string>(&prependExe)->default_value(""),             "Prepend with the command all exe tags")
            ("output,o",     bpo::value<string>(&outputTopo)->default_value("topology.xml"), "Output topology filepath");
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
            if (!monitorTask.empty()) {
                auto errorMonitorTask{ recoCollection->addElement<CTopoTask>("ErrorMonitorTask") };
                errorMonitorTask->initFromXML(monitorTask);
                errorMonitorTask->setExe(prependExe + errorMonitorTask->getExe());
            }
            // Add new requirement - one Reco DPL collection per host
            auto recoInstanceRequirement{ recoCollection->addRequirement("RecoInstanceRequirement") };
            recoInstanceRequirement->setRequirementType(CTopoRequirement::EType::MaxInstancesPerHost);
            recoInstanceRequirement->setValue("1");
            if (!recoWorkerNodes.empty()) {
                // Add new worker node name requirement
                auto recoWnRequirement{ recoCollection->addRequirement("RecoWnRequirement") };
                recoWnRequirement->setRequirementType(CTopoRequirement::EType::WnName);
                recoWnRequirement->setValue(recoWorkerNodes);
            } else if (!recoAgentGroup.empty()) {
                // Add new worker node name requirement
                auto recoAgentGroupRequirement{ recoCollection->addRequirement("RecoAgentGroupRequirement") };
                recoAgentGroupRequirement->setRequirementType(CTopoRequirement::EType::GroupName);
                recoAgentGroupRequirement->setValue(recoAgentGroup);
            }
        }

        // Calibration
        if (calibTopos.size() > 0) {
            // check for duplicate collection names
            std::sort(calibTopos.begin(), calibTopos.end());
            auto it = std::unique(calibTopos.begin(), calibTopos.end());
            if (it != calibTopos.end()) {
                stringstream ss;
                ss << "duplicate calibration collection(s) found: ";
                do {
                    ss << std::quoted(*it) << ", ";
                    ++it;
                } while (it != calibTopos.end());
                string str(ss.str());
                str.erase(str.size() - 2);
                throw runtime_error(str);
            }

            for (const auto& calibTopo : calibTopos) {
                cout << "Creating calibration collection '" << calibTopo << "'..." << endl;
                auto calibCollection{ creator.getMainGroup()->addElement<CTopoCollection>("DPL") };
                calibCollection->initFromXML(calibTopo);
                calibCollection->setName(removeExtension(calibTopo));

                // Add stderr monitor task and initialize it from XML topology file
                if (!monitorTask.empty()) {
                    auto errorMonitorTask{ calibCollection->addElement<CTopoTask>("ErrorMonitorTask") };
                    errorMonitorTask->initFromXML(monitorTask);
                    errorMonitorTask->setExe(prependExe + errorMonitorTask->getExe());
                }

                if (!calibWorkerNodes.empty()) {
                    // Add new requirement - calibration worker node name
                    auto calibWnRequirement{ calibCollection->addRequirement("CalibWnRequirement") };
                    calibWnRequirement->setRequirementType(CTopoRequirement::EType::WnName);
                    calibWnRequirement->setValue(calibWorkerNodes);
                } else if (!calibAgentGroup.empty()) {
                    // Add new requirement - calibration worker node name
                    auto calibAgentGroupRequirement{ calibCollection->addRequirement("CalibAgentGroupRequirement") };
                    calibAgentGroupRequirement->setRequirementType(CTopoRequirement::EType::GroupName);
                    calibAgentGroupRequirement->setValue(calibAgentGroup);
                }
            }
        }

        // Save topology to the oputput file
        creator.save(outputTopo);
        cout << "New DDS topology successfully created and saved to a file " << quoted(outputTopo) << endl;

        if (recoTopos.size() > 0 && recoN > 0 && recoNmin > 0) {
            if (recoNmin > recoN) {
                cerr << "--nmin (" << recoNmin << ") cannot be larger than --n (" << recoN << "), aborting" << endl;
                return EXIT_FAILURE;
            }

            CTopoVars vars;
            vars.initFromXML(outputTopo);
            // "odc_nmin_" is a convention. ODC will look for this prefix, to map reco collections to their nmin value
            vars.add("odc_nmin_RecoGroup", std::to_string(recoNmin));
            vars.saveToXML(outputTopo);
        }

        // Validate created topology
        // Create a topology from the output file
        CTopology topo(outputTopo);
        cout << "DDS topology " << quoted(topo.getName()) << " successfully opened from file " << quoted(topo.getFilepath()) << endl;
    } catch (exception& _e) {
        cerr << _e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
