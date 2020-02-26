// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "CliHelper.h"

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

void CCliHelper::addInitializeOptions(boost::program_options::options_description& _options,
                                      const SInitializeParams& _defaultParams,
                                      SInitializeParams& _params)
{
    _options.add_options()(
        "runid", bpo::value<runID_t>(&_params.m_runID)->default_value(_defaultParams.m_runID), "Run ID");
}

void CCliHelper::addActivateOptions(bpo::options_description& _options,
                                    const SActivateParams& _defaultParams,
                                    SActivateParams& _params)
{
    _options.add_options()("topo",
                           bpo::value<string>(&_params.m_topologyFile)->default_value(_defaultParams.m_topologyFile),
                           "Topology filepath");
}

void CCliHelper::addUpscaleOptions(bpo::options_description& _options,
                                   const SUpdateParams& _defaultParams,
                                   SUpdateParams& _params)
{
    _options.add_options()("uptopo",
                           bpo::value<string>(&_params.m_topologyFile)->default_value(_defaultParams.m_topologyFile),
                           "Topology filepath of the upscale");
}

void CCliHelper::addDownscaleOptions(bpo::options_description& _options,
                                     const SUpdateParams& _defaultParams,
                                     SUpdateParams& _params)
{
    _options.add_options()("downtopo",
                           bpo::value<string>(&_params.m_topologyFile)->default_value(_defaultParams.m_topologyFile),
                           "Topology filepath of the downscale request");
}

void CCliHelper::addSubmitOptions(bpo::options_description& _options,
                                  const SSubmitParams& _defaultParams,
                                  SSubmitParams& _params)
{
    _options.add_options()("rms,r",
                           bpo::value<string>(&_params.m_rmsPlugin)->default_value(_defaultParams.m_rmsPlugin),
                           "Defines a destination resource management system plug-in.");
    _options.add_options()("config,c",
                           bpo::value<string>(&_params.m_configFile)->default_value(_defaultParams.m_configFile),
                           "A plug-in's configuration file. It can be used to provide additional RMS options");
    _options.add_options()("agents",
                           bpo::value<size_t>(&_params.m_numAgents)->default_value(_defaultParams.m_numAgents),
                           "Number of DDS agents");
    _options.add_options()("slots",
                           bpo::value<size_t>(&_params.m_numSlots)->default_value(_defaultParams.m_numSlots),
                           "Number of slots per DDS agent");
}

void CCliHelper::addHostOptions(bpo::options_description& _options, const string& _defaultHost, string& _host)
{
    _options.add_options()("host", bpo::value<string>(&_host)->default_value(_defaultHost), "Server address");
}

void CCliHelper::addLogOptions(boost::program_options::options_description& _options,
                               const CLogger::SConfig& _defaultConfig,
                               CLogger::SConfig& _config)
{
    _options.add_options()(
        "logdir", bpo::value<string>(&_config.m_logDir)->default_value(_defaultConfig.m_logDir), "Log files directory");
    _options.add_options()("severity",
                           bpo::value<ESeverity>(&_config.m_severity)->default_value(_defaultConfig.m_severity),
                           "Log severity level");
}