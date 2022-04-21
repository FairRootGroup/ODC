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

class PluginManager
{
  public:
    // Plugin map <plugin name, cmd>
    using PluginMap_t = std::map<std::string, std::string>;

    void registerPlugin(const std::string& plugin, const std::string& cmd)
    {
        // Check if plugin already registered
        auto it = mPlugins.find(plugin);
        if (it != mPlugins.end()) {
            std::stringstream ss;
            ss << "Failed to register resource plugin " << std::quoted(plugin) << ". A different plugin with the same name is already registered.";
            throw std::runtime_error(ss.str());
        }

        // Extract the path from command line
        std::vector<std::string> args{ boost::program_options::split_unix(cmd) };
        std::string path{ args.empty() ? "" : args.front() };

        try {
            // Throws an exception if path doesn't exist
            // Check if plugin exists at path and not a directory
            const boost::filesystem::path pluginPath(boost::filesystem::canonical(boost::filesystem::path(path)));
            if (boost::filesystem::is_directory(pluginPath)) {
                std::stringstream ss;
                ss << "Failed to register resource plugin " << std::quoted(plugin) << ". Specified path " << pluginPath << " is a directory.";
                throw std::runtime_error(ss.str());
            }

            std::string correctedCmd{ cmd };
            boost::algorithm::replace_first(correctedCmd, path, pluginPath.string());
            OLOG(info) << "Register resource plugin " << std::quoted(plugin) << " as " << std::quoted(correctedCmd);
            mPlugins.insert(make_pair(plugin, correctedCmd));
        } catch (const std::exception& _e) {
            std::stringstream ss;
            ss << "Failed to register resource plugin " << std::quoted(plugin) << ": " << _e.what();
            throw std::runtime_error(ss.str());
        }
    }

    std::string execPlugin(const std::string& plugin, const std::string& resources, const std::string& partitionID, uint64_t runNr) const
    {
        // Check if plugin exists
        auto it = mPlugins.find(plugin);
        if (it == mPlugins.end()) {
            throw std::runtime_error(toString("Failed to execute resource plugin ", std::quoted(plugin), ". Plugin not found."));
        }

        // Execute plugin
        const std::string cmd{ toString(it->second, " --res ", std::quoted(resources), " --id ", std::quoted(partitionID)) };
        const std::chrono::seconds timeout{ 30 };
        std::string out;
        std::string err;
        int exitCode{ EXIT_SUCCESS };
        OLOG(debug, partitionID, runNr) << "Executing plugin " << std::quoted(cmd);
        execute(cmd, timeout, &out, &err, &exitCode);

        if (exitCode != EXIT_SUCCESS) {
            throw std::runtime_error(toString("Plugin ", std::quoted(plugin), " execution failed with exit code: ", exitCode, "; error message: ", err));
        }
        return out;
    }
    bool isPluginRegistered(const std::string& plugin) const { return mPlugins.find(plugin) != mPlugins.end(); }

  private:
    PluginMap_t mPlugins; ///< Plugin map
};

} // namespace odc::core

#endif /* defined(__ODC__PluginManager__) */
