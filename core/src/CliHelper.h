// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliHelper__
#define __ODC__CliHelper__

// ODC
#include "ControlService.h"
#include "Logger.h"
#include "PluginManager.h"
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

namespace odc::core
{
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

        //
        // Helpers
        //
        /// Function used to check that '_opt1' and '_opt2' are not specified at the same time
        static void conflictingOptions(const boost::program_options::variables_map& _vm,
                                       const std::string& _opt1,
                                       const std::string& _opt2);

        /// \brief Fills `CCliHelper::SBatchOptions::m_outputCmds`
        static void batchCmds(const boost::program_options::variables_map& _vm,
                              bool _batch,
                              SBatchOptions& _batchOptions);

        //
        // Generic options
        //
        static void addHelpOptions(boost::program_options::options_description& _options);
        static void addVersionOptions(boost::program_options::options_description& _options);
        static void addSyncOptions(boost::program_options::options_description& _options, bool& _sync);
        static void addHostOptions(boost::program_options::options_description& _options, std::string& _host);
        static void addLogOptions(boost::program_options::options_description& _options, CLogger::SConfig& _config);
        static void addTimeoutOptions(boost::program_options::options_description& _options, size_t& _timeout);
        static void addOptions(boost::program_options::options_description& _options, SBatchOptions& _batchOptions);
        static void addBatchOptions(boost::program_options::options_description& _options,
                                    SBatchOptions& _batchOptions,
                                    bool& _batch);
        static void addResourcePluginOptions(boost::program_options::options_description& _options,
                                             CPluginManager::PluginMap_t& _pluginMap);
        static void addRequestTriggersOptions(boost::program_options::options_description& _options,
                                              CPluginManager::PluginMap_t& _pluginMap);
        static void addOptions(boost::program_options::options_description& _options, SSleepOptions& _sleepOptions);
        static void addRestoreOptions(boost::program_options::options_description& _options, std::string& _restoreId);

        //
        // Option Parsing
        //
        static void parsePluginMapOptions(const boost::program_options::variables_map& _vm,
                                          CPluginManager::PluginMap_t& _pluginMap,
                                          const std::string& _option);

        //
        // Request specific options
        //
        static void addOptions(boost::program_options::options_description& _options, partitionID_t& _partitionID);
        static void addOptions(boost::program_options::options_description& _options, SInitializeParams& _params);
        static void addOptions(boost::program_options::options_description& _options, SActivateParams& _params);
        static void addOptions(boost::program_options::options_description& _options, SUpdateParams& _params);
        static void addOptions(boost::program_options::options_description& _options, SSubmitParams& _params);
        static void addOptions(boost::program_options::options_description& _options, SDeviceParams& _params);
        static void addOptions(boost::program_options::options_description& _options, SSetPropertiesParams& _params);
        static void addOptions(boost::program_options::options_description& _options, SStatusParams& _params);

        //
        // Extra step of options parsing
        //
        template <typename... RequestParams_t>
        static void parseOptions(const boost::program_options::variables_map& /*_vm*/, RequestParams_t&&... /*_params*/)
        {
            // Default implementation does nothing
        }

        static void parseOptions(const boost::program_options::variables_map& _vm,
                                 partitionID_t& _partitionID,
                                 SSetPropertiesParams& _params);
        static void parseOptions(const boost::program_options::variables_map& _vm, CCliHelper::SBatchOptions& _params);
    };
} // namespace odc::core

#endif /* defined(__ODC__CliHelper__) */
