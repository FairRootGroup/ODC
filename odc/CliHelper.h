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
#include <odc/Controller.h>
#include <odc/Logger.h>
#include <odc/PluginManager.h>
#include <string>
#include <vector>

namespace odc::core {

class CliHelper
{
  public:
    struct BatchOptions
    {
        std::vector<std::string> mCmds;
        std::string mCmdsFilepath;
        std::vector<std::string> mOutputCmds; ///> Filled after parseOptions
    };

    struct SleepOptions
    {
        size_t mMs; ///> Sleep time in milliseconds
    };

    /// Checks that '_opt1' and '_opt2' are not specified at the same time
    static void conflictingOptions(const boost::program_options::variables_map& vm, const std::string& opt1, const std::string& opt2);

    /// \brief Fills BatchOptions::m_outputCmds
    static void batchCmds(const boost::program_options::variables_map& vm, bool batch, BatchOptions& batchOptions);

    // Generic options

    static void addHelpOptions(   boost::program_options::options_description& options);
    static void addVersionOptions(boost::program_options::options_description& options);
    static void addSyncOptions(   boost::program_options::options_description& options, bool& sync);
    static void addHostOptions(   boost::program_options::options_description& options, std::string& host);
    static void addLogOptions(    boost::program_options::options_description& options, CLogger::SConfig& config);
    static void addTimeoutOptions(boost::program_options::options_description& options, size_t& timeout);
    static void addOptions(       boost::program_options::options_description& options, BatchOptions& batchOptions);
    static void addBatchOptions(  boost::program_options::options_description& options, BatchOptions& batchOptions, bool& batch);
    static void addOptions(       boost::program_options::options_description& options, SleepOptions& sleepOptions);
    static void addRestoreOptions(boost::program_options::options_description& options, std::string& restoreId);

    // Request specific options

    static void addPartitionOptions(boost::program_options::options_description& options, std::string& partitionID);
    static void addOptions(boost::program_options::options_description& options, CommonParams& common);
    static void addOptions(boost::program_options::options_description& options, InitializeParams& params);
    static void addOptions(boost::program_options::options_description& options, ActivateParams& params);
    static void addOptions(boost::program_options::options_description& options, UpdateParams& params);
    static void addOptions(boost::program_options::options_description& options, SubmitParams& params);
    static void addOptions(boost::program_options::options_description& options, DeviceParams& params);
    static void addOptions(boost::program_options::options_description& options, SetPropertiesParams& params);
    static void addOptions(boost::program_options::options_description& options, StatusParams& params);

    // Plugin options

    static void addResourcePluginOptions(boost::program_options::options_description& options, CPluginManager::PluginMap_t& pluginMap);
    static void addRequestTriggersOptions(boost::program_options::options_description& options, CPluginManager::PluginMap_t& pluginMap);

    // Options parsing

    static void parsePluginMapOptions(const boost::program_options::variables_map& vm, CPluginManager::PluginMap_t& pluginMap, const std::string& option);

    template<typename... RequestParams_t>
    static void parseOptions(const boost::program_options::variables_map& /*vm*/, RequestParams_t&&... /*params*/) {} // Default implementation does nothing
    static void parseOptions(const boost::program_options::variables_map& vm, std::string& partitionID, SetPropertiesParams& params);
    static void parseOptions(const boost::program_options::variables_map& vm, CliHelper::BatchOptions& params);
};

} // namespace odc::core

#endif /* defined(__ODC__CliHelper__) */
