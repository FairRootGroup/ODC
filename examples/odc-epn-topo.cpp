/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <dds/dds.h>
#include <dds/TopoVars.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <iomanip>
#include <iostream>
#include <set>

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
            auto task = _collection->addElement<CTopoTask>(element->getName());
            task->initFromXML(filepath);
            task->setExe(_prependExe + task->getExe());
        }
    }
}

string getFilenameComponent(const string& str)
{
    auto pos = str.find_last_of("/");
    if (pos == string::npos) {
        return str.substr(0, str.find_last_of("."));
    } else {
        return str.substr(str.find_last_of("/") + 1, str.find_last_of("."));
    }
}

int main(int argc, char** argv)
{
    try {
        string ddTopo;
        vector<string> recoTopos;
        string recoZone;
        size_t recoN;
        size_t recoNmin;
        vector<string> calibTopos;
        string calibZone;
        string monitorTask;
        string outputTopo;
        string prependExe;

        bpo::options_description options("odc-epn-topo options");
        options.add_options()
            ("help,h",       "Produce help message")
            ("dd",           bpo::value<string>(&ddTopo)->default_value(""),                 "Filepath to XML topology of Data Distribution")
            ("reco,r",       bpo::value<vector<string>>(&recoTopos)->multitoken(),           "Space separated list of filepaths of reconstruction XML topologies")
            ("recozone",     bpo::value<string>(&recoZone)->default_value(""),               "Name of the reconstruction zone")
            ("mon",          bpo::value<string>(&monitorTask)->default_value(""),            "Filepath to XML topology of a stderr monitor tool")
            ("n",            bpo::value<size_t>(&recoN)->default_value(1),                   "Multiplicator for the reconstruction group")
            ("nmin",         bpo::value<size_t>(&recoNmin)->default_value(0),                "Minimum number of working reco groups before failing a run")
            ("calib,c",      bpo::value<vector<string>>(&calibTopos)->multitoken(),          "Space separated list of <filepath>:<ncores> of calibration XML topologies "
                                                                                             "(example: '--calib calib1.xml:20 calib2.xml:10' for two calibration "
                                                                                             "collections with 20 and 10 cores respectively)")
            ("calibzone",    bpo::value<string>(&calibZone)->default_value(""),              "Name of the calibration zone")
            ("prependexe,p", bpo::value<string>(&prependExe)->default_value(""),             "Prepend with the command all exe tags")
            ("output,o",     bpo::value<string>(&outputTopo)->default_value("topology.xml"), "Output topology filepath");
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            cout << options;
            return EXIT_SUCCESS;
        }

        CTopoCreator creator;

        // Reconstruction
        if ((!ddTopo.empty() || recoTopos.size() > 0) && recoN > 0) {
            cout << "Creating reconstruction collection..." << endl;
            // New Reco group containing Reco DPL collections
            auto recoGroup = creator.getMainGroup()->addElement<CTopoGroup>("RecoGroup");
            // Set required number of Reco DPL collections
            recoGroup->setN(recoN);
            // New Reco DPL collection containing TfBuilder task and all tasks from Reco DPL collections
            auto recoCol = recoGroup->addElement<CTopoCollection>("RecoCollection");
            // Add TfBuilder task and initialize it from XML topology file
            if (!ddTopo.empty()) {
                auto ddTask = recoCol->addElement<CTopoTask>("TfBuilderTask");
                ddTask->initFromXML(ddTopo);
                ddTask->setExe(prependExe + ddTask->getExe());
            }
            // Combine all tasks from reco DPL collections to a reco collection
            combineDPLCollections(recoTopos, recoCol, prependExe);

            // Add stderr monitor task and initialize it from XML topology file
            if (!monitorTask.empty()) {
                auto errorMonitorTask = recoCol->addElement<CTopoTask>("ErrorMonitorTask");
                errorMonitorTask->initFromXML(monitorTask);
                errorMonitorTask->setExe(prependExe + errorMonitorTask->getExe());
            }
            // limit number of instances per host
            auto recoInstanceReqs = recoCol->addRequirement("RecoInstanceRequirement");
            recoInstanceReqs->setRequirementType(CTopoRequirement::EType::MaxInstancesPerHost);
            recoInstanceReqs->setValue("1");
            if (!recoZone.empty()) {
                // reconstruction agent group name
                auto recoAgentGroupReq = recoCol->addRequirement("RecoAgentGroupRequirement");
                recoAgentGroupReq->setRequirementType(CTopoRequirement::EType::GroupName);
                recoAgentGroupReq->setValue(recoZone);
                // reconstruction zone name
                auto recoZoneReq = recoCol->addRequirement("odc_zone_reco");
                recoZoneReq->setRequirementType(CTopoRequirement::EType::Custom);
                recoZoneReq->setValue(recoZone);
            }

            cout << " ..name: " << recoCol->getName() << endl;
        }

        // Calibration
        if (calibTopos.size() > 0) {
            set<string> calibs;

            for (const auto& calibTopo : calibTopos) {
                cout << "Creating calibration collection from " << quoted(calibTopo) << "..." << endl;

                size_t ncores = 0;
                string filename;

                auto pos = calibTopo.rfind(':');
                if (pos == string::npos) {
                    filename = calibTopo;
                } else {
                    filename = calibTopo.substr(0, pos);
                    ncores = stoi(calibTopo.substr(pos + 1));
                }
                string calibName = getFilenameComponent(filename);
                // check for duplicates
                auto [it, inserted] = calibs.emplace(calibName);
                if (!inserted) {
                    stringstream ss;
                    ss << "duplicate collection found: " << calibTopo;
                    throw runtime_error(ss.str());
                }

                cout << " ..name: " << calibName << ", ncores: " << ncores << ", zone: " << endl;

                auto calibCol = creator.getMainGroup()->addElement<CTopoCollection>("DPL");
                calibCol->initFromXML(filename);
                calibCol->setName(calibName);

                // prepend exe of tasks with the given value
                if (!prependExe.empty()) {
                    const auto& elements{ calibCol->getElements() };
                    for (const auto& element : elements) {
                        if (element->getType() == CTopoBase::EType::TASK) {
                            CTopoTask::Ptr_t task = dynamic_pointer_cast<CTopoTask>(element);
                            task->setExe(prependExe + task->getExe());
                        }
                    }
                }

                // add number of cores requirement, if specified
                if (ncores > 0) {
                    string requirementName("odc_ncores_" + calibName);
                    auto calibNCoresReq = calibCol->addRequirement(requirementName);
                    calibNCoresReq->setRequirementType(CTopoRequirement::EType::Custom);
                    calibNCoresReq->setValue(to_string(ncores));
                }

                // Add stderr monitor task and initialize it from XML topology file
                if (!monitorTask.empty()) {
                    auto errorMonitorTask = calibCol->addElement<CTopoTask>("ErrorMonitorTask");
                    errorMonitorTask->initFromXML(monitorTask);
                    errorMonitorTask->setExe(prependExe + errorMonitorTask->getExe());
                }

                if (!calibZone.empty()) {
                    // calibration agent group name
                    auto calibAgentGroupReq = calibCol->addRequirement("CalibAgentGroupRequirement" + to_string(calibs.size()));
                    calibAgentGroupReq->setRequirementType(CTopoRequirement::EType::GroupName);
                    string calibGroup(calibZone + to_string(calibs.size()));
                    calibAgentGroupReq->setValue(calibGroup);
                    // calibration zone name
                    auto calibZoneReq = calibCol->addRequirement("odc_zone_calib");
                    calibZoneReq->setRequirementType(CTopoRequirement::EType::Custom);
                    calibZoneReq->setValue(calibZone);
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
            vars.add("odc_nmin_RecoGroup", to_string(recoNmin));
            vars.saveToXML(outputTopo);
        }

        // Validate created topology -create a topology from the output file
        CTopology topo(outputTopo);
        cout << "DDS topology " << quoted(topo.getName()) << " successfully opened from file " << quoted(topo.getFilepath()) << endl;
    } catch (exception& _e) {
        cerr << _e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
