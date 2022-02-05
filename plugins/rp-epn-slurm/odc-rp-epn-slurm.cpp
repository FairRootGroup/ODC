// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// STD
#include <iostream>
// BOOST
#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/json_parser.hpp>
// ODC
#include "Logger.h"
#include "Version.h"

using namespace std;
namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;
namespace bpt = boost::property_tree;
using namespace odc;
using namespace odc::core;

struct SResource
{
    SResource(const bpt::ptree& _pt)
    {
        // Do some basic validation of the input tree.
        // Only valid tags are allowed.
        set<string> validTags{ "zone", "n" };
        for (const auto& v : _pt) {
            if (validTags.count(v.first.data()) == 0) {
                stringstream ss;
                ss << "Failed to init from property tree. Unknown key " << quoted(v.first.data());
                throw runtime_error(ss.str());
            }
        }
        m_zone = _pt.get<string>("zone", "online");
        m_n = _pt.get<size_t>("n", 1);
    }

    string m_zone;
    size_t m_n = 0;
};

struct SResources
{
    SResources(const string& _res)
    {
        bpt::ptree pt;
        stringstream ss;
        ss << _res;
        bpt::read_json(ss, pt);

        try {
            // Single resource
            m_resources.push_back(SResource(pt));
        } catch (const exception& _e) {
            // Array of resources
            auto rpt{ pt.get_child_optional("") };
            if (rpt) {
                for (const auto& v : rpt.get()) {
                    m_resources.push_back(SResource(v.second));
                }
            }
        }
    }

    vector<SResource> m_resources;
};

int main(int argc, char** argv)
{
    try {
        string resources;
        CLogger::SConfig logConfig;
        string partitionID;
        string slurmConfig;
        size_t numSlots = 1;

        string defaultLogDir{ smart_path(string("$HOME/.ODC/log")) };

        bpo::options_description opts("odc-rp-epn-slurm options");
        opts.add_options()
            ("help,h", "Help message")
            ("version,v", "Print version")
            ("res", bpo::value<string>(&resources)->default_value("{\"zone\":\"online\",\"n\":1}"), "Resource description in JSON format")
            ("logdir", bpo::value<string>(&logConfig.m_logDir)->default_value(defaultLogDir), "Log files directory")
            ("severity", bpo::value<ESeverity>(&logConfig.m_severity)->default_value(ESeverity::info), "Log severity level")
            ("infologger", bpo::bool_switch(&logConfig.m_infologger)->default_value(false), "Enable InfoLogger (ODC needs to be compiled with InfoLogger support)")
            ("id", bpo::value<string>(&partitionID)->default_value(""), "Partition ID")
            ("slurm-config", bpo::value<string>(&slurmConfig)->default_value(""), "Path to slurm config file")
            ("slots", bpo::value<size_t>(&numSlots)->default_value(1), "Number of slots for each worker node");

        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(opts).run(), vm);
        bpo::notify(vm);

        try {
            CLogger::instance().init(logConfig);
        } catch (exception& _e) {
            cerr << "Can't initialize log: " << _e.what() << endl;
            return EXIT_FAILURE;
        }

        if (vm.count("help")) {
            cout << opts;
            return EXIT_SUCCESS;
        }

        if (vm.count("version")) {
            OLOG(clean) << ODC_VERSION;
            return EXIT_SUCCESS;
        }

        OLOG(info, partitionID, 0) << "Starting epn slurm plugin";

        SResources res(resources);

        int numNodes = 0;

        for (const auto& r : res.m_resources) {
            numNodes += r.m_n;
        }

        stringstream ss;
        int requiredSlots(numNodes * numSlots);
        ss << "<rms>slurm</rms>";
        if (!slurmConfig.empty()) {
            ss << "<configFile>" << slurmConfig << "</configFile>";
        }
        ss << "<agents>" << numNodes << "</agents>"; // number of agents (assuming it is equals to number of nodes)
        ss << "<slots>" << numSlots << "</slots>"; // number of slots per agent
        ss << "<requiredSlots>" << requiredSlots << "</requiredSlots>"; // total number of required slots

        OLOG(info, partitionID, 0) << ss.str();
        OLOG(clean, partitionID, 0) << ss.str();

        OLOG(info, partitionID, 0) << "Finishing epn slurm plugin";
    } catch (exception& _e) {
        cerr << _e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
