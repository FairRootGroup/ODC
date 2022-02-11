/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ODC
#include <odc/PluginManager.h>
#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/Process.h>
// BOOST
#include <boost/filesystem.hpp>
#include <boost/program_options/parsers.hpp>

using namespace odc;
using namespace odc::core;
using namespace std;
namespace fs = boost::filesystem;
namespace bpo = boost::program_options;
namespace ba = boost::algorithm;

void CPluginManager::registerPlugin(const std::string& _plugin, const std::string& _cmd)
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

    // Extract the path from command line
    std::vector<std::string> args{ bpo::split_unix(_cmd) };
    std::string path{ args.empty() ? "" : args.front() };

    try
    {
        // Throws an exception if path doesn't exist
        // Check if plugin exists at path and not a directory
        const fs::path pluginPath(fs::canonical(fs::path(path)));
        if (fs::is_directory(pluginPath))
        {
            stringstream ss;
            ss << "Failed to register resource plugin " << quoted(_plugin) << ". Specified path " << pluginPath
               << " is a directory.";
            throw runtime_error(ss.str());
        }

        string correctedCmd{ _cmd };
        ba::replace_first(correctedCmd, path, pluginPath.string());
        OLOG(info) << "Register resource plugin " << quoted(_plugin) << " as " << quoted(correctedCmd);
        m_plugins.insert(make_pair(_plugin, correctedCmd));
    }
    catch (const exception& _e)
    {
        stringstream ss;
        ss << "Failed to register resource plugin " << quoted(_plugin) << ": " << _e.what();
        throw runtime_error(ss.str());
    }
}

std::string CPluginManager::execPlugin(const string& _plugin,
                                       const string& _resources,
                                       const partitionID_t& _partitionID,
                                       runNr_t _runNr) const
{
    // Check if plugin exists
    auto it = m_plugins.find(_plugin);
    if (it == m_plugins.end())
    {
        throw runtime_error(toString("Failed to execute resource plugin ", quoted(_plugin), ". Plugin not found."));
    }

    // Execute plugin
    const string cmd{ toString(it->second, " --res ", std::quoted(_resources), " --id ", std::quoted(_partitionID)) };
    const std::chrono::seconds timeout{ 30 };
    string out;
    string err;
    int exitCode{ EXIT_SUCCESS };
    OLOG(debug, _partitionID, _runNr) << "Executing plugin " << std::quoted(cmd);
    execute(cmd, timeout, &out, &err, &exitCode);

    if (exitCode != EXIT_SUCCESS)
    {
        throw runtime_error(toString(
            "Plugin ", quoted(_plugin), " execution failed with exit code: ", exitCode, "; error message: ", err));
    }
    return out;
}

bool CPluginManager::isPluginRegistered(const std::string& _plugin) const
{
    return m_plugins.find(_plugin) != m_plugins.end();
}
