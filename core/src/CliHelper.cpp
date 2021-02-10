// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "CliHelper.h"
#include "BuildConstants.h"
// BOOST
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

//
// Generic options
//

void CCliHelper::addHelpOptions(boost::program_options::options_description& _options)
{
    _options.add_options()("help,h", "Produce help message");
}

void CCliHelper::addVersionOptions(boost::program_options::options_description& _options)
{
    _options.add_options()("version,v", "Print version");
}

void CCliHelper::addTimeoutOptions(boost::program_options::options_description& _options, size_t& _timeout)
{
    _options.add_options()("timeout", bpo::value<size_t>(&_timeout)->default_value(30), "Timeout of requests in sec");
}

void CCliHelper::addHostOptions(bpo::options_description& _options, string& _host)
{
    _options.add_options()("host", bpo::value<string>(&_host)->default_value("localhost:50051"), "Server address");
}

void CCliHelper::addLogOptions(boost::program_options::options_description& _options, CLogger::SConfig& _config)
{
    _options.add_options()("logdir", bpo::value<string>(&_config.m_logDir)->default_value(""), "Log files directory");
    _options.add_options()(
        "severity", bpo::value<ESeverity>(&_config.m_severity)->default_value(ESeverity::info), "Log severity level");
}

void CCliHelper::addBatchOptions(boost::program_options::options_description& _options,
                                 std::vector<std::string>& _cmds,
                                 bool& _batch,
                                 std::vector<partitionID_t>& _partitions)
{
    using boost::algorithm::join;

    _options.add_options()("batch", bpo::bool_switch(&_batch)->default_value(false), "Non interactive batch mode");

    string defaultUpTopo{ kODCDataDir + "/ex-dds-topology-infinite-up.xml" };
    string defaultDownTopo{ kODCDataDir + "/ex-dds-topology-infinite-down.xml" };
    string upCmd{ ".upscale --topo " + defaultUpTopo };
    string downCmd{ ".downscale --topo " + defaultDownTopo };
    vector<string> defaultCmds{ ".init", ".submit", ".activate", ".config", ".start", ".stop", upCmd,  ".start",
                                ".stop", downCmd,   ".start",    ".stop",   ".reset", ".term", ".down" };
    string defaultCmdsStr{ join(defaultCmds, " ") };
    _options.add_options()(
        "cmds",
        bpo::value<std::vector<std::string>>(&_cmds)->multitoken()->default_value(defaultCmds, defaultCmdsStr),
        "Array of command to be executed in batch mode");

    vector<partitionID_t> defaultPartitions{};
    string defaultPartitionsStr{ join(defaultPartitions, " ") };
    _options.add_options()(
        "prts",
        bpo::value<std::vector<partitionID_t>>(&_partitions)
            ->multitoken()
            ->default_value(defaultPartitions, defaultPartitionsStr),
        "Array of partition IDs. If set than each command in batch mode will be executed for each partition.");
}

//
// Request specific options
//

void CCliHelper::addOptions(boost::program_options::options_description& _options, partitionID_t& _partitionID)
{
    _options.add_options()("id", bpo::value<partitionID_t>(&_partitionID)->default_value(""), "Partition ID");
}

void CCliHelper::addOptions(boost::program_options::options_description& _options, SInitializeParams& _params)
{
    _options.add_options()("sid", bpo::value<string>(&_params.m_sessionID)->default_value(""), "Session ID of DDS");
}

void CCliHelper::addOptions(bpo::options_description& _options, SActivateParams& _params)
{
    string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite.xml");
    _options.add_options()(
        "topo", bpo::value<string>(&_params.m_topologyFile)->default_value(defaultTopo), "Topology filepath");
}

void CCliHelper::addOptions(bpo::options_description& _options, SUpdateParams& _params)
{
    string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-up.xml");
    // string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-down.xml");
    _options.add_options()(
        "topo", bpo::value<string>(&_params.m_topologyFile)->default_value(defaultTopo), "Topology filepath");
}

void CCliHelper::addOptions(bpo::options_description& _options, SSubmitParams& _params)
{
    _options.add_options()("rms,r",
                           bpo::value<string>(&_params.m_rmsPlugin)->default_value("localhost"),
                           "Defines a destination resource management system plug-in.");
    _options.add_options()("config,c",
                           bpo::value<string>(&_params.m_configFile)->default_value(""),
                           "A plug-in's configuration file. It can be used to provide additional RMS options");
    _options.add_options()(
        "agents", bpo::value<size_t>(&_params.m_numAgents)->default_value(1), "Number of DDS agents");
    _options.add_options()(
        "slots", bpo::value<size_t>(&_params.m_numSlots)->default_value(36), "Number of slots per DDS agent");
}

void CCliHelper::addOptions(boost::program_options::options_description& _options, SDeviceParams& _params)
{
    _options.add_options()("path", bpo::value<string>(&_params.m_path)->default_value(""), "Topology path of devices");
    _options.add_options()(
        "detailed", bpo::bool_switch(&_params.m_detailed)->default_value(false), "Detailed reply of devices");
}

void CCliHelper::addOptions(boost::program_options::options_description& _options, SSetPropertiesParams& _params)
{
    _options.add_options()(
        "path", bpo::value<string>(&_params.m_path)->default_value(""), "Path for a set property request");

    const SSetPropertiesParams::Properties_t props{ { "key1", "value1" }, { "key2", "value2" } };
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

//
// Extra step of options parsing
//

void CCliHelper::parseOptions(const boost::program_options::variables_map& _vm, SSetPropertiesParams& _params)
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
        _params.m_properties = { { "key1", "value1" }, { "key2", "value2" } };
    }
}
