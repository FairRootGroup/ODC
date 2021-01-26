// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliHelper__
#define __ODC__CliHelper__

// ODC
#include "ControlService.h"
#include "Logger.h"
// BOOST
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

namespace odc::core
{
    class CCliHelper
    {
      public:
        //
        // Generic options
        //
        static void addHelpOptions(boost::program_options::options_description& _options);
        static void addHostOptions(boost::program_options::options_description& _options, std::string& _host);
        static void addLogOptions(boost::program_options::options_description& _options, CLogger::SConfig& _config);
        static void addTimeoutOptions(boost::program_options::options_description& _options, size_t& _timeout);
        static void addBatchOptions(boost::program_options::options_description& _options,
                                    std::vector<std::string>& _cmds,
                                    bool& _batch,
                                    std::vector<partitionID_t>& _partitions);

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

        //
        // Extra step of options parsing
        //
        template <typename... RequestParams_t>
        static void parseOptions(const boost::program_options::variables_map& _vm, RequestParams_t&&... _params)
        {
            // Default implementation does nothing
        }

        static void parseOptions(const boost::program_options::variables_map& _vm, SSetPropertiesParams& _params);
    };
} // namespace odc::core

#endif /* defined(__ODC__CliHelper__) */
