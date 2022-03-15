/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__CliHelper__
#define __ODC__CliHelper__

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstddef>
#include <odc/ControlService.h>
#include <odc/Logger.h>
#include <odc/PluginManager.h>
#include <string>
#include <vector>

namespace odc::core {

class CCliHelper
{
  public:
    struct SBatchOptions
    {
        std::vector<std::string> m_cmds;
        std::string m_cmdsFilepath;
        std::vector<std::string> m_outputCmds; ///> Filled after parseOptions
    };

    struct SSleepOptions
    {
        size_t m_ms; ///> Sleep time in milliseconds
    };

    /// Checks that '_opt1' and '_opt2' are not specified at the same time
    static void conflictingOptions(const boost::program_options::variables_map& vm, const std::string& opt1, const std::string& opt2);

    /// \brief Fills SBatchOptions::m_outputCmds
    static void batchCmds(const boost::program_options::variables_map& vm, bool batch, SBatchOptions& batchOptions);

    // Generic options

    static void addHelpOptions(   boost::program_options::options_description& options);
    static void addVersionOptions(boost::program_options::options_description& options);
    static void addSyncOptions(   boost::program_options::options_description& options, bool& sync);
    static void addHostOptions(   boost::program_options::options_description& options, std::string& host);
    static void addLogOptions(    boost::program_options::options_description& options, CLogger::SConfig& config);
    static void addTimeoutOptions(boost::program_options::options_description& options, size_t& timeout);
    static void addOptions(       boost::program_options::options_description& options, SBatchOptions& batchOptions);
    static void addBatchOptions(  boost::program_options::options_description& options, SBatchOptions& batchOptions, bool& batch);
    static void addOptions(       boost::program_options::options_description& options, SSleepOptions& sleepOptions);
    static void addRestoreOptions(boost::program_options::options_description& options, std::string& restoreId);

    // Request specific options

    static void addPartitionOptions(boost::program_options::options_description& options, std::string& partitionID);
    static void addOptions(boost::program_options::options_description& options, CommonParams& common);
    static void addOptions(boost::program_options::options_description& options, SInitializeParams& params);
    static void addOptions(boost::program_options::options_description& options, SActivateParams& params);
    static void addOptions(boost::program_options::options_description& options, SUpdateParams& params);
    static void addOptions(boost::program_options::options_description& options, SSubmitParams& params);
    static void addOptions(boost::program_options::options_description& options, SDeviceParams& params);
    static void addOptions(boost::program_options::options_description& options, SetPropertiesParams& params);
    static void addOptions(boost::program_options::options_description& options, SStatusParams& params);

    // Plugin options

    static void addResourcePluginOptions(boost::program_options::options_description& options, CPluginManager::PluginMap_t& pluginMap);
    static void addRequestTriggersOptions(boost::program_options::options_description& options, CPluginManager::PluginMap_t& pluginMap);

    // Options parsing

    static void parsePluginMapOptions(const boost::program_options::variables_map& vm, CPluginManager::PluginMap_t& pluginMap, const std::string& option);

    template<typename... RequestParams_t>
    static void parseOptions(const boost::program_options::variables_map& /*vm*/, RequestParams_t&&... /*params*/) {} // Default implementation does nothing
    static void parseOptions(const boost::program_options::variables_map& vm, std::string& partitionID, SetPropertiesParams& params);
    static void parseOptions(const boost::program_options::variables_map& vm, CCliHelper::SBatchOptions& params);
};

} // namespace odc::core

#endif /* defined(__ODC__CliHelper__) */
