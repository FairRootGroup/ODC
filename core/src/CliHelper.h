// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliHelper__
#define __ODC__CliHelper__

// ODC
#include "ControlService.h"
// BOOST
#include <boost/program_options/options_description.hpp>

namespace odc
{
    namespace core
    {
        class CCliHelper
        {

          public:
            static void printDescription();

            static void addInitializeOptions(boost::program_options::options_description& _options,
                                             const SInitializeParams& _defaultParams,
                                             SInitializeParams& _params);
            static void addActivateOptions(boost::program_options::options_description& _options,
                                           const SActivateParams& _defaultParams,
                                           SActivateParams& _params);
            static void addUpscaleOptions(boost::program_options::options_description& _options,
                                          const SUpdateParams& _defaultParams,
                                          SUpdateParams& _params);
            static void addDownscaleOptions(boost::program_options::options_description& _options,
                                            const SUpdateParams& _defaultParams,
                                            SUpdateParams& _params);
            static void addSubmitOptions(boost::program_options::options_description& _options,
                                         const SSubmitParams& _defaultParams,
                                         SSubmitParams& _params);
            static void addHostOptions(boost::program_options::options_description& _options,
                                       const std::string& _defaultHost,
                                       std::string& _host);
        };
    } // namespace core
} // namespace odc

#endif /* defined(__ODC__CliHelper__) */
