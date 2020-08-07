// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "CliHelper.h"
// BOOST
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

void CCliHelper::addTimeoutOptions(boost::program_options::options_description& _options,
                                   size_t _defaultTimeout,
                                   size_t& _timeout)
{
    _options.add_options()(
        "timeout", bpo::value<size_t>(&_timeout)->default_value(_defaultTimeout), "Timeout of requests in sec");
}

void CCliHelper::addInitializeOptions(boost::program_options::options_description& _options,
                                      const SInitializeParams& _defaultParams,
                                      SInitializeParams& _params)
{
    _options.add_options()(
        "runid", bpo::value<runID_t>(&_params.m_runID)->default_value(_defaultParams.m_runID), "Run ID");
    _options.add_options()("sid",
                           bpo::value<string>(&_params.m_sessionID)->default_value(_defaultParams.m_sessionID),
                           "Session ID of DDS");
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

void CCliHelper::addDeviceOptions(boost::program_options::options_description& _options,
                                  const SDeviceParams& _defaultRecoParams,
                                  SDeviceParams& _recoParams,
                                  const SDeviceParams& _defaultQCParams,
                                  SDeviceParams& _qcParams)
{
    _options.add_options()("rpath",
                           bpo::value<string>(&_recoParams.m_path)->default_value(_defaultRecoParams.m_path),
                           "Topology path of reco devices");
    _options.add_options()("rdetailed",
                           bpo::bool_switch(&_recoParams.m_detailed)->default_value(_defaultRecoParams.m_detailed),
                           "Detailed reply of reco devices");
    _options.add_options()("qpath",
                           bpo::value<string>(&_qcParams.m_path)->default_value(_defaultQCParams.m_path),
                           "Topology path of QC devices");
    _options.add_options()("qdetailed",
                           bpo::bool_switch(&_qcParams.m_detailed)->default_value(_defaultQCParams.m_detailed),
                           "Detailed reply of QC devices");
}

void CCliHelper::addSetPropertiesOptions(boost::program_options::options_description& _options,
                                         const SSetPropertiesParams& _defaultParams,
                                         SSetPropertiesParams& _params)
{
    _options.add_options()("ppath",
                           bpo::value<string>(&_params.m_path)->default_value(_defaultParams.m_path),
                           "Path for a set property request");

    const auto& props(_defaultParams.m_properties);
    vector<string> defaults;
    transform(
        props.begin(), props.end(), back_inserter(defaults), [](const SSetPropertiesParams::Property_t& _p) -> string {
            return _p.first + ":" + _p.second;
        });
    string defaultsStr{ boost::algorithm::join(defaults, " ") };
    _options.add_options()("prop",
                           bpo::value<vector<string>>()->multitoken()->default_value(defaults, defaultsStr),
                           "Key-value pairs for a set properties request ( key1:value1 key2:value2 )");
}

void CCliHelper::parseProperties(const boost::program_options::variables_map& _vm,
                                 const SSetPropertiesParams& _defaultParams,
                                 SSetPropertiesParams& _params)
{
    if (_vm.count("prop"))
    {
        const auto& kvp(_vm["prop"].as<vector<string>>());
        SSetPropertiesParams::Properties_t props;
        for (const auto& v : kvp)
        {
            vector<string> strs;
            boost::split(strs, v, boost::is_any_of(":"));
            if (strs.size() == 2)
            {
                props.push_back({ strs[0], strs[1] });
            }
            else
            {
                throw runtime_error("Wrong property format for string '" + v + "'. Use 'key:value'.");
            }
        }
        _params.m_properties = props;
    }
    else
    {
        _params.m_properties = _defaultParams.m_properties;
    }
}

void CCliHelper::addBatchOptions(boost::program_options::options_description& _options,
                                 const std::vector<std::string>& _defaultCmds,
                                 std::vector<std::string>& _cmds,
                                 bool _defaultBatch,
                                 bool& _batch)
{
    _options.add_options()(
        "batch", bpo::bool_switch(&_batch)->default_value(_defaultBatch), "Non interactive batch mode");
    string defaultsStr{ boost::algorithm::join(_defaultCmds, " ") };
    _options.add_options()(
        "cmds",
        bpo::value<std::vector<std::string>>(&_cmds)->multitoken()->default_value(_defaultCmds, defaultsStr),
        "Array of command to be executed in batch mode");
}
