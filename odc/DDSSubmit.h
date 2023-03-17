/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_DDSSUBMIT
#define ODC_CORE_DDSSUBMIT

#include <odc/BuildConstants.h>
#include <odc/Logger.h>
#include <odc/Params.h>
#include <odc/PluginManager.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <algorithm>
#include <chrono>
#include <ostream>
#include <string>
#include <vector>

namespace odc::core {

struct ZoneGroup
{
    int32_t n;
    int nCores;
    std::string agentGroup;
};

struct DDSSubmitParams
{
    DDSSubmitParams() {}
    DDSSubmitParams(const DDSSubmitParams&) = default;
    DDSSubmitParams(DDSSubmitParams&&) = default;
    DDSSubmitParams& operator=(const DDSSubmitParams&) = default;
    DDSSubmitParams& operator=(DDSSubmitParams&&) = default;

    DDSSubmitParams(const boost::property_tree::ptree& pt)
    {
        // Only valid tags are allowed.
        std::set<std::string> validTags{ "rms", "configFile", "envFile", "agents", "slots", "zone" };
        for (const auto& v : pt) {
            if (validTags.count(v.first.data()) == 0) {
                throw std::runtime_error(toString("Failed to init from property tree. Unknown key ", std::quoted(v.first.data())));
            }
        }
        mRMS = pt.get<std::string>("rms", "");
        mZone = pt.get<std::string>("zone", "");
        // by default agent group equals the zone name (but can be overwritten via the topology)
        mAgentGroup = pt.get<std::string>("zone", "");
        mConfigFile = pt.get<std::string>("configFile", "");
        mEnvFile = pt.get<std::string>("envFile", "");
        mNumAgents = pt.get<int32_t>("agents", -1);
        mMinAgents = 0;
        mNumSlots = pt.get<size_t>("slots", 0);
        // number of cores is set dynamically from the topology (if provided), not from the initial resource definition
        mNumCores = 0;
    }

    std::string mRMS;        ///< RMS plugin of DDS
    std::string mZone;       ///< Zone name
    std::string mAgentGroup; ///< Agent group name
    std::string mConfigFile; ///< Path to the configuration file of the RMS plugin
    std::string mEnvFile;    ///< Path to the environment file
    int32_t mNumAgents = 0;  ///< Number of DDS agents
    size_t mMinAgents = 0;   ///< Minimum number of DDS agents
    size_t mNumSlots = 0;    ///< Number of slots per DDS agent
    size_t mNumCores = 0;    ///< Number of cores

    // \brief ostream operator.
    friend std::ostream& operator<<(std::ostream& os, const DDSSubmitParams& params)
    {
        return os << "DDSSubmitParams:"
            << " rms: "         << params.mRMS
            << ", zone: "       << params.mZone
            << ", agentGroup: " << params.mAgentGroup
            << ", numAgents: "  << params.mNumAgents
            << ", minAgents: "  << params.mMinAgents
            << ", numSlots: "   << params.mNumSlots
            << ", numCores: "   << params.mNumCores
            << ", configFile: " << std::quoted(params.mConfigFile)
            << ", envFile: "    << std::quoted(params.mEnvFile);
    }
};

class DDSSubmit : public PluginManager
{
  public:
    DDSSubmit()
    {
        registerDefaultPlugin("odc-rp-same");
    }

    std::vector<DDSSubmitParams> makeParams(const std::string& plugin,
                                   const std::string& resources,
                                   const CommonParams& common,
                                   const std::map<std::string, std::vector<ZoneGroup>>& zoneInfo,
                                   std::chrono::seconds timeout)
    {
        std::vector<DDSSubmitParams> params;
        OLOG(info, common) << "Starting " << plugin << " plugin...";
        std::stringstream ss{ execPlugin(plugin, resources, common.mPartitionID, common.mRunNr, timeout) };
        OLOG(info, common) << "Finishing " << plugin << " plugin. Plugin output:\n" << ss.str();
        boost::property_tree::ptree pt;
        boost::property_tree::read_xml(ss, pt, boost::property_tree::xml_parser::no_comments);
        // check if parameters are children of <submit> tag(s), or flat
        size_t nSubmitTags = pt.count("submit");
        if (nSubmitTags < 1) {
            params.emplace_back(pt);
        } else {
            for (const auto& [name, element] : pt) {
                if (name != "submit") {
                    throw std::runtime_error(toString("Failed to init from property tree. Unknown top level tag ", std::quoted(name)));
                }
                params.emplace_back(element);
            }
        }

        // extend parameters, if nCores is provided
        for (const auto& [zoneNameSB, zoneGroups] : zoneInfo) {
            std::string zoneName(zoneNameSB);
            // check if the zone defined in the topology was provided to the plugin...
            auto parameterSet = find_if(params.begin(), params.end(), [&zoneName](const auto& p){ return p.mZone == zoneName; });
            if (parameterSet == params.end()) {
                throw std::runtime_error(toString("Zone '", zoneName, "' not found. Check --zones setting of the resource plugin."));
            } else {
                // ...if yes, it means numCores was provided, so we overwrite
                // the core number and agent group for the found parameter set
                parameterSet->mNumCores = zoneGroups.at(0).nCores;
                parameterSet->mAgentGroup = zoneGroups.at(0).agentGroup;
                // for core-based scheduling, set number of agents to 1
                if (parameterSet->mNumCores != 0) {
                    parameterSet->mNumAgents = 1;
                }
                DDSSubmitParams tempParams = *parameterSet;
                // for the rest of agent groups (if present), add them as new parameter sets
                for (size_t i = 1; i < zoneGroups.size(); ++i) {
                    params.push_back(tempParams);
                    params.back().mNumCores = zoneGroups.at(i).nCores;
                    params.back().mAgentGroup = zoneGroups.at(i).agentGroup;
                }
            }
        }

        // if no 'n' was provided, and the selected zone did not get any core scheduling, remove it
        params.erase(
            std::remove_if(params.begin(), params.end(), [](const auto& p) {
                return p.mNumAgents == -1; }),
            params.end()
        );

        return params;
    }

  private:
    void registerDefaultPlugin(const std::string& name)
    {
        try {
            boost::filesystem::path pluginPath{ kODCBinDir };
            pluginPath /= name;
            registerPlugin(name, pluginPath.string());
        } catch (const std::exception& e) {
            OLOG(error) << "Unable to register default resource plugin " << std::quoted(name) << ": " << e.what();
        }
    }
};

} // namespace odc::core

#endif /* defined(ODC_CORE_DDSSUBMIT) */
