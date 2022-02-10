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
#include "PluginManager.h"
#include "BuildConstants.h"
#include "Logger.h"

namespace odc::core {

class CDDSSubmit : public CPluginManager
{
  public:
    /// \brief DDS submit parameters
    struct SParams
    {
        SParams() {}

        /// \brief Constructor with arguments
        SParams(const std::string& _rmsPlugin, const std::string& _configFile, const std::string& _agentGroup, size_t _numAgents, size_t _numSlots, size_t _numRequiredSlots)
            : m_rmsPlugin(_rmsPlugin)
            , m_configFile(_configFile)
            , m_agentGroup(_agentGroup)
            , m_numAgents(_numAgents)
            , m_numSlots(_numSlots)
            , m_numRequiredSlots(_numRequiredSlots)
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
        void initFromPT(const boost::property_tree::ptree& _pt)
        {
            // TODO: FIXME: <configContent> is not yet defined
            // To support it we need to create a temporary file with configuration content and use it as config file.

            // Only valid tags are allowed.
            std::set<std::string> validTags{ "rms", "configFile", "agents", "slots", "requiredSlots", "agentGroup" };
            for (const auto& v : _pt) {
                if (validTags.count(v.first.data()) == 0) {
                    throw std::runtime_error(toString("Failed to init from property tree. Unknown key ", std::quoted(v.first.data())));
                }
            }
            m_rmsPlugin = _pt.get<std::string>("rms", "");
            m_configFile = _pt.get<std::string>("configFile", "");
            m_numAgents = _pt.get<size_t>("agents", 0);
            m_numSlots = _pt.get<size_t>("slots", 0);
            m_numRequiredSlots = _pt.get<size_t>("requiredSlots", 0);
            m_agentGroup = _pt.get<std::string>("agentGroup", "");
        }

        std::string m_rmsPlugin;        ///< RMS plugin of DDS
        std::string m_configFile;       ///< Path to the configuration file of the RMS plugin
        std::string m_agentGroup;        ///< Agent group name
        size_t m_numAgents{ 0 };        ///< Number of DDS agents
        size_t m_numSlots{ 0 };         ///< Number of slots per DDS agent
        size_t m_numRequiredSlots{ 0 }; ///< Wait for the required number of slots become active

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SParams& _params)
        {
            return _os << "CDDSSubmit::SParams: rmsPlugin=" << std::quoted(_params.m_rmsPlugin)
                       << "; numAgents=" << _params.m_numAgents
                       << "; agentGroup=" << _params.m_agentGroup
                       << "; numSlots=" << _params.m_numSlots
                       << "; configFile=" << std::quoted(_params.m_configFile)
                       << "; numRequiredSlots=" << _params.m_numRequiredSlots;
        }
    };

    CDDSSubmit()
    {
        registerDefaultPlugin("odc-rp-same");
    }

    std::vector<SParams> makeParams(const std::string& _plugin, const std::string& _resources, const partitionID_t& _partitionID, runNr_t _runNr)
    {
        std::vector<SParams> params;
        std::stringstream ss{ execPlugin(_plugin, _resources, _partitionID, _runNr) };
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
    void registerDefaultPlugin(const std::string& _name)
    {
        try {
            boost::filesystem::path pluginPath{ kODCBinDir };
            pluginPath /= _name;
            registerPlugin(_name, pluginPath.string());
        } catch (const std::exception& _e) {
            OLOG(error) << "Unable to register default resource plugin " << std::quoted(_name) << ": " << _e.what();
        }
    }

  public:
    // Shared pointer
    using Ptr_t = std::shared_ptr<CDDSSubmit>;
};

} // namespace odc::core

#endif /* defined(__ODC__DDSSubmit__) */
