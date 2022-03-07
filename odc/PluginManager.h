/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__PluginManager__
#define __ODC__PluginManager__

#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/PluginManager.h>
#include <odc/Process.h>

#include <boost/filesystem.hpp>
#include <boost/program_options/parsers.hpp>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace odc::core {

class CPluginManager
{
  public:
    // Plugin map <plugin name, cmd>
    using PluginMap_t = std::map<std::string, std::string>;

    void registerPlugin(const std::string& _plugin, const std::string& _cmd)
    {
        // Check if plugin already registered
        auto it = m_plugins.find(_plugin);
        if (it != m_plugins.end()) {
            std::stringstream ss;
            ss << "Failed to register resource plugin " << std::quoted(_plugin) << ". A different plugin with the same name is already registered.";
            throw std::runtime_error(ss.str());
        }

        // Extract the path from command line
        std::vector<std::string> args{ boost::program_options::split_unix(_cmd) };
        std::string path{ args.empty() ? "" : args.front() };

        try {
            // Throws an exception if path doesn't exist
            // Check if plugin exists at path and not a directory
            const boost::filesystem::path pluginPath(boost::filesystem::canonical(boost::filesystem::path(path)));
            if (boost::filesystem::is_directory(pluginPath)) {
                std::stringstream ss;
                ss << "Failed to register resource plugin " << std::quoted(_plugin) << ". Specified path " << pluginPath << " is a directory.";
                throw std::runtime_error(ss.str());
            }

            std::string correctedCmd{ _cmd };
            boost::algorithm::replace_first(correctedCmd, path, pluginPath.string());
            OLOG(info) << "Register resource plugin " << std::quoted(_plugin) << " as " << std::quoted(correctedCmd);
            m_plugins.insert(make_pair(_plugin, correctedCmd));
        } catch (const std::exception& _e) {
            std::stringstream ss;
            ss << "Failed to register resource plugin " << std::quoted(_plugin) << ": " << _e.what();
            throw std::runtime_error(ss.str());
        }
    }

    std::string execPlugin(const std::string& _plugin, const std::string& _resources, const std::string& _partitionID, uint64_t _runNr) const
    {
        // Check if plugin exists
        auto it = m_plugins.find(_plugin);
        if (it == m_plugins.end()) {
            throw std::runtime_error(toString("Failed to execute resource plugin ", std::quoted(_plugin), ". Plugin not found."));
        }

        // Execute plugin
        const std::string cmd{ toString(it->second, " --res ", std::quoted(_resources), " --id ", std::quoted(_partitionID)) };
        const std::chrono::seconds timeout{ 30 };
        std::string out;
        std::string err;
        int exitCode{ EXIT_SUCCESS };
        OLOG(debug, _partitionID, _runNr) << "Executing plugin " << std::quoted(cmd);
        execute(cmd, timeout, &out, &err, &exitCode);

        if (exitCode != EXIT_SUCCESS) {
            throw std::runtime_error(toString("Plugin ", std::quoted(_plugin), " execution failed with exit code: ", exitCode, "; error message: ", err));
        }
        return out;
    }
    bool isPluginRegistered(const std::string& _plugin) const { return m_plugins.find(_plugin) != m_plugins.end(); }

  private:
    PluginMap_t m_plugins; ///< Plugin map
};

} // namespace odc::core

#endif /* defined(__ODC__PluginManager__) */
