/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/


#include <odc/Logger.h>
#include <odc/Version.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;
namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;
namespace bpt = boost::property_tree;
using namespace odc;
using namespace odc::core;

struct Resource
{
    Resource(const bpt::ptree& pt)
    {
        // Do some basic validation of the input tree.
        // Only valid tags are allowed.
        set<string> validTags{ "zone", "n", "nmin" };
        for (const auto& v : pt) {
            if (validTags.count(v.first.data()) == 0) {
                stringstream ss;
                ss << "Failed to init from property tree. Unknown key " << quoted(v.first.data());
                throw runtime_error(ss.str());
            }
        }
        mZone = pt.get<string>("zone", "online");
        mN = pt.get<size_t>("n", 1);
        mNmin = pt.get<size_t>("nmin", 0);
    }

    string mZone;
    size_t mN = 0;
    size_t mNmin = 0;
};

struct Resources
{
    Resources(const string& res)
    {
        bpt::ptree pt;
        stringstream ss;
        ss << res;
        bpt::read_json(ss, pt);

        try {
            // Single resource
            mResources.push_back(Resource(pt));
        } catch (const exception& _e) {
            // Array of resources
            auto rpt{ pt.get_child_optional("") };
            if (rpt) {
                for (const auto& v : rpt.get()) {
                    mResources.push_back(Resource(v.second));
                }
            }
        }
    }

    vector<Resource> mResources;
};

struct ZoneConfig
{
    size_t numSlots;
    std::string slurmCfgPath;
    std::string envCfgPath;
};

std::map<std::string, ZoneConfig> getZoneConfig(const std::vector<std::string>& zonesStr)
{
    std::map<std::string, ZoneConfig> result;

    for (const auto& z : zonesStr) {
        std::vector<std::string> zoneCfg;
        boost::algorithm::split(zoneCfg, z, boost::algorithm::is_any_of(":"));
        if (zoneCfg.size() != 4) {
            OLOG(error) << "Provided zones configuration has incorrect format. Expected <name>:<numSlots>:<slurmCfgPath>:<envCfgPath>.";
            throw std::runtime_error("Provided zones configuration has incorrect format. Expected <name>:<numSlots>:<slurmCfgPath>:<envCfgPath>.");
        }
        result.emplace(zoneCfg.at(0), ZoneConfig{ std::stoul(zoneCfg.at(1)), zoneCfg.at(2), zoneCfg.at(3) });
    }

    return result;
}

int main(int argc, char** argv)
{
    try {
        string resources;
        Logger::Config logConfig;
        string partitionID;
        std::vector<std::string> zonesStr;

        string defaultLogDir{ smart_path(string("$HOME/.ODC/log")) };

        bpo::options_description opts("odc-rp-epn-slurm options");
        opts.add_options()
            ("help,h", "Help message")
            ("version,v", "Print version")
            ("res", bpo::value<string>(&resources)->default_value("{\"zone\":\"online\",\"n\":1}"), "Resource description in JSON format")
            ("logdir", bpo::value<string>(&logConfig.mLogDir)->default_value(defaultLogDir), "Log files directory")
            ("severity", bpo::value<ESeverity>(&logConfig.mSeverity)->default_value(ESeverity::info), "Log severity level")
            ("infologger", bpo::bool_switch(&logConfig.mInfologger)->default_value(false), "Enable InfoLogger (ODC needs to be compiled with InfoLogger support)")
            ("id", bpo::value<string>(&partitionID)->default_value(""), "ECS partition ID")
            ("zones", bpo::value<std::vector<std::string>>(&zonesStr)->multitoken()->composing(), "Zones in <name>:<numSlots>:<slurmCfgPath>:<envCfgPath> format");

        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(opts).run(), vm);
        bpo::notify(vm);

        try {
            Logger::instance().init(logConfig);
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

        Resources res(resources);
        std::map<std::string, ZoneConfig> zones{getZoneConfig(zonesStr)};

        for (const auto& r : res.mResources) {
            stringstream ss;
            const auto& zone = zones.at(r.mZone);
            ss << "<submit>"
               << "<rms>slurm</rms>";
            if (!zone.slurmCfgPath.empty()) {
                ss << "<configFile>" << zone.slurmCfgPath << "</configFile>";
            }
            if (!zone.envCfgPath.empty()) {
                ss << "<envFile>" << zone.envCfgPath << "</envFile>";
            }
            if (r.mNmin != 0) {
                ss << "<minAgents>" << r.mNmin << "</minAgents>";
            }
            ss << "<agents>" << r.mN << "</agents>" // number of agents (assuming it is equals to number of nodes)
               << "<zone>" << r.mZone << "</zone>" // zone
               << "<slots>" << zone.numSlots << "</slots>" // number of slots per agent
               << "</submit>";

            OLOG(info, partitionID, 0) << ss.str();
            std::cout << ss.str() << std::endl;
        }

        OLOG(info, partitionID, 0) << "Finishing epn slurm plugin";
    } catch (exception& e) {
        cerr << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
