// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "CliHelper.h"
#include "BuildConstants.h"
#include "CmdsFile.h"
// BOOST
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

void CCliHelper::conflictingOptions(const boost::program_options::variables_map& _vm,
                                    const std::string& _opt1,
                                    const std::string& _opt2)
{
    if (_vm.count(_opt1) && !_vm[_opt1].defaulted() && _vm.count(_opt2) && !_vm[_opt2].defaulted())
    {
        stringstream ss;
        ss << "Conflicting options " << quoted(_opt1) << " and " << quoted(_opt2);
        throw runtime_error(ss.str());
    }
}

vector<string> CCliHelper::batchCmds(const boost::program_options::variables_map& _vm,
                                     const std::vector<std::string>& _cmds,
                                     const std::string& _cmdsFilepath,
                                     const bool _batch)
{
    CCliHelper::conflictingOptions(_vm, "cmds", "cf");
    if (_batch)
    {
        if ((_vm.count("cmds") && _vm["cmds"].defaulted() && _vm.count("cf") && _vm["cf"].defaulted()) ||
            (_vm.count("cmds") && !_vm["cmds"].defaulted()))
            return _cmds;
        else if (_vm.count("cf") && !_vm["cf"].defaulted())
            return CCmdsFile::getCmds(_cmdsFilepath);
    }
    return vector<string>();
}

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
                                 std::string& _cmdsFilepath,
                                 bool& _batch)
{
    using boost::algorithm::join;

    _options.add_options()("batch", bpo::bool_switch(&_batch)->default_value(false), "Non interactive batch mode");

    const vector<partitionID_t> defaultPartitions{ "a1b2c3", "d4e5f6" };
    const string defaultUpTopo{ kODCDataDir + "/ex-dds-topology-infinite-up.xml" };
    const string defaultDownTopo{ kODCDataDir + "/ex-dds-topology-infinite-down.xml" };
    const string upStr{ ".upscale --topo " + defaultUpTopo };
    const string downStr{ ".downscale --topo " + defaultDownTopo };
    const vector<string> defaultCmds{ ".init", ".submit", ".activate", ".config", ".start", ".stop", upStr,  ".start",
                                      ".stop", downStr,   ".start",    ".stop",   ".reset", ".term", ".down" };

    vector<string> cmds;
    for (const auto& cmd : defaultCmds)
    {
        for (const auto& prt : defaultPartitions)
        {
            stringstream ss;
            ss << cmd + " --id " + prt;
            cmds.push_back(ss.str());
        }
    }

    const string cmdsStr{ join(cmds, " ") };
    _options.add_options()("cmds",
                           bpo::value<std::vector<std::string>>(&_cmds)->multitoken()->default_value(cmds, cmdsStr),
                           "Array of command to be executed in batch mode");

    const string defaultConfig{ kODCDataDir + "/cmds.cfg" };
    _options.add_options()("cf",
                           bpo::value<std::string>(&_cmdsFilepath)->default_value(defaultConfig),
                           "Config file containing an array of command to be executed in batch mode");
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
    _options.add_options()(
        "plugin,p", bpo::value<string>(&_params.m_plugin)->default_value("odc-rp-same"), "ODC resource plugin name.");
    _options.add_options()(
        "resources,r",
        bpo::value<string>(&_params.m_resources)
            ->default_value("<rms>localhost</rms><agents>1</agents><slots>36</slots><requiredSlots>36</requiredSlots>"),
        "A resource description for a corresponding ODC resource plugin.");
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
// Resource plugins
//

void CCliHelper::addResourcePluginOptions(boost::program_options::options_description& _options,
                                          CDDSSubmit::PluginMap_t& _pluginMap)
{
    _options.add_options()(
        "rp", bpo::value<vector<string>>()->multitoken(), "Register resource plugins ( name1:path1 name2:path2 )");
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

void CCliHelper::parseResourcePluginOptions(const boost::program_options::variables_map& _vm,
                                            CDDSSubmit::PluginMap_t& _pluginMap)
{
    if (_vm.count("rp"))
    {
        const auto& kvp(_vm["rp"].as<vector<string>>());
        _pluginMap.clear();
        for (const auto& v : kvp)
        {
            vector<string> strs;
            boost::split(strs, v, boost::is_any_of(":"));
            if (strs.size() == 2)
            {
                _pluginMap.insert(make_pair(strs[0], strs[1]));
            }
            else
            {
                throw runtime_error("Wrong resource plugin format for string '" + v + "'. Use 'name:path'.");
            }
        }
    }
    else
    {
        _pluginMap.clear();
    }
}
