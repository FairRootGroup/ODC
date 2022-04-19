/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__DDSSubmit__
#define __ODC__DDSSubmit__

// STD
#include <ostream>
#include <string>
// BOOST
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
// ODC
#include <odc/BuildConstants.h>
#include <odc/Logger.h>
#include <odc/PluginManager.h>

namespace odc::core {

class CDDSSubmit : public CPluginManager
{
  public:
    /// \brief DDS submit parameters
    struct SParams
    {
        SParams() {}

        /// \brief Constructor with arguments
        SParams(const std::string& rmsPlugin, const std::string& configFile, const std::string& agentGroup, size_t numAgents, size_t numSlots, size_t numRequiredSlots)
            : mRMSPlugin(rmsPlugin)
            , mConfigFile(configFile)
            , mAgentGroup(agentGroup)
            , mNumAgents(numAgents)
            , mNumSlots(numSlots)
            , mNumRequiredSlots(numRequiredSlots)
        {}

        /// \brief Initialization of structure from an XML file
        // void initFromXML(const std::string& _filepath);
        /// \brief Initialization of structure from an XML stream
        // void initFromXML(std::istream& _stream)
        // {
        //     boost::property_tree::ptree pt;
        //     boost::property_tree::read_xml(_stream, pt, boost::property_tree::xml_parser::no_comments);
        //     initFromPT(pt);
        // }
        /// \brief Initialization of structure from property tree
        void initFromPT(const boost::property_tree::ptree& pt)
        {
            // TODO: FIXME: <configContent> is not yet defined
            // To support it we need to create a temporary file with configuration content and use it as config file.

            // Only valid tags are allowed.
            std::set<std::string> validTags{ "rms", "configFile", "agents", "slots", "requiredSlots", "agentGroup" };
            for (const auto& v : pt) {
                if (validTags.count(v.first.data()) == 0) {
                    throw std::runtime_error(toString("Failed to init from property tree. Unknown key ", std::quoted(v.first.data())));
                }
            }
            mRMSPlugin = pt.get<std::string>("rms", "");
            mConfigFile = pt.get<std::string>("configFile", "");
            mNumAgents = pt.get<size_t>("agents", 0);
            mNumSlots = pt.get<size_t>("slots", 0);
            mNumRequiredSlots = pt.get<size_t>("requiredSlots", 0);
            mAgentGroup = pt.get<std::string>("agentGroup", "");
        }

        std::string mRMSPlugin;        ///< RMS plugin of DDS
        std::string mConfigFile;       ///< Path to the configuration file of the RMS plugin
        std::string mAgentGroup;        ///< Agent group name
        size_t mNumAgents{ 0 };        ///< Number of DDS agents
        size_t mNumSlots{ 0 };         ///< Number of slots per DDS agent
        size_t mNumRequiredSlots{ 0 }; ///< Wait for the required number of slots become active

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& os, const SParams& params)
        {
            return os << "CDDSSubmit::SParams: rmsPlugin=" << std::quoted(params.mRMSPlugin)
                       << "; numAgents=" << params.mNumAgents
                       << "; agentGroup=" << params.mAgentGroup
                       << "; numSlots=" << params.mNumSlots
                       << "; configFile=" << std::quoted(params.mConfigFile)
                       << "; numRequiredSlots=" << params.mNumRequiredSlots;
        }
    };

    CDDSSubmit()
    {
        registerDefaultPlugin("odc-rp-same");
    }

    std::vector<SParams> makeParams(const std::string& plugin, const std::string& resources, const std::string& partitionID, uint64_t runNr)
    {
        std::vector<SParams> params;
        std::stringstream ss{ execPlugin(plugin, resources, partitionID, runNr) };
        boost::property_tree::ptree pt;
        boost::property_tree::read_xml(ss, pt, boost::property_tree::xml_parser::no_comments);
        // check if parameters are children of <submit> tag(s), or flat
        size_t nSubmitTags = pt.count("submit");
        if (nSubmitTags < 1) {
            params.emplace_back();
            params.back().initFromPT(pt);
            return params;
        } else {
            for (const auto& [name, element] : pt) {
                if (name != "submit") {
                    throw std::runtime_error(toString("Failed to init from property tree. Unknown top level tag ", std::quoted(name)));
                }
                params.emplace_back();
                params.back().initFromPT(element);
            }
        }

        return params;
    }

  private:
    void registerDefaultPlugin(const std::string& name)
    {
        try {
            boost::filesystem::path pluginPath{ kODCBinDir };
            pluginPath /= name;
            registerPlugin(name, pluginPath.string());
        } catch (const std::exception& _e) {
            OLOG(error) << "Unable to register default resource plugin " << std::quoted(name) << ": " << _e.what();
        }
    }
};

} // namespace odc::core

#endif /* defined(__ODC__DDSSubmit__) */
