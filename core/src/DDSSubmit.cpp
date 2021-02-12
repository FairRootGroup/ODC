// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "DDSSubmit.h"
#include "Logger.h"
// STD
#include <cstdlib>
#include <iomanip>
#include <ostream>
#include <sstream>
// BOOST
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace odc;
using namespace odc::core;
using namespace std;
namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

//
// CDDSSubmit::SParams
//

CDDSSubmit::SParams::SParams()
{
}

CDDSSubmit::SParams::SParams(const string& _rmsPlugin, const string& _configFile, size_t _numAgents, size_t _numSlots)
    : m_rmsPlugin(_rmsPlugin)
    , m_configFile(_configFile)
    , m_numAgents(_numAgents)
    , m_numSlots(_numSlots)
{
}

void CDDSSubmit::SParams::initFromXML(istream& _stream)
{
    pt::ptree pt;
    pt::read_xml(_stream, pt, pt::xml_parser::no_comments);
    initFromPT(pt);
}

void CDDSSubmit::SParams::initFromPT(const pt::ptree& _pt)
{
    // TODO: FIXME: define required fields?

    // Resource description format.
    // Top tag is <res>
    // XML tags:
    // <rms> - name of the DDS RMS plugin
    // <configFile> - path to the configuration file
    // <configContent> - content of the configuration file
    // <agents> - number of agents
    // <slots> - number of slots per agent
    // <requiredSlots> - required number of total active slots

    // TODO: FIXME: <configContent> is not yet defined
    // To support it we need to create a temporary file with configuration content and use it as config file.

    m_rmsPlugin = _pt.get<string>("res.rms", "");
    m_configFile = _pt.get<string>("res.configFile", "");
    m_numAgents = _pt.get<size_t>("res.agents", 0);
    m_numSlots = _pt.get<size_t>("res.slots", 0);
    m_requiredNumSlots = _pt.get<size_t>("res.requiredSlots", 0);
}

//
// CDDSSubmit
//

void CDDSSubmit::registerPlugin(const std::string& _plugin, const std::string& _path)
{
    // Check if plugin already registered
    auto it = m_plugins.find(_plugin);
    if (it != m_plugins.end())
    {
        stringstream ss;
        ss << "Failed to register resource plugin " << quoted(_plugin)
           << ". A different plugin with the same name is already registered.";
        throw runtime_error(ss.str());
    }

    // Check if plugin axists at path and not a directory
    // Throws an exception if path doesn't exist
    const fs::path pluginPath(fs::canonical(fs::path(_path)));

    if (fs::is_directory(pluginPath))
    {
        stringstream ss;
        ss << "Failed to register resource plugin " << quoted(_plugin) << ". Specified path " << pluginPath
           << " is a directory.";
        throw runtime_error(ss.str());
    }

    OLOG(ESeverity::info) << "Register resource plugin " << quoted(_plugin) << " at path "
                          << quoted(pluginPath.string());
    m_plugins.insert(make_pair(_plugin, pluginPath.string()));
}

CDDSSubmit::SParams CDDSSubmit::makeParams(const string& _plugin, const string& _resources)
{
    // TODO: FIXME: initial implementation assumes that _resources already containes XML description in the proper
    // format.

    // TODO: FIXME:
    // Check if plugin with the same name has already been registered
    // Check if path exists and is executable
    // Execute plugin and wait for an answer
    // Implement timeout for plugin execution
    // Save configuration to a local temporary file if <configContent> if present or use file directly if <configFile>
    // is present.

    //    // Check if plugin exists
    //    auto it = m_plugins.find(_plugin);
    //    if (it == m_plugins.end())
    //    {
    //        stringstream ss;
    //        ss << "Failed to execute resource plugin " << quoted(_plugin) << ". Plugin not found.";
    //        throw runtime_error(ss.str());
    //    }
    //
    //    const string cmd {it->second + " --res " + "\"" + _resources + "\""};

    CDDSSubmit::SParams params;
    stringstream ss{ _resources };
    params.initFromXML(ss);

    return params;
}

//
// Misc
//

namespace odc::core
{
    ostream& operator<<(ostream& _os, const CDDSSubmit::SParams& _params)
    {
        return _os << "CDDSSubmit::SParams: rmsPlugin=" << quoted(_params.m_rmsPlugin)
                   << "; numAgents=" << _params.m_numAgents << "; numSlots=" << _params.m_numSlots
                   << "; configFile=" << quoted(_params.m_configFile)
                   << "; requiredNumSlots=" << _params.m_requiredNumSlots;
    }
} // namespace odc::core
