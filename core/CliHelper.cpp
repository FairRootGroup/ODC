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

void CCliHelper::conflictingOptions(const bpo::variables_map& _vm, const string& _opt1, const string& _opt2)
{
    if (_vm.count(_opt1) && !_vm[_opt1].defaulted() && _vm.count(_opt2) && !_vm[_opt2].defaulted()) {
        stringstream ss;
        ss << "Conflicting options " << quoted(_opt1) << " and " << quoted(_opt2);
        throw runtime_error(ss.str());
    }
}

void CCliHelper::batchCmds(const bpo::variables_map& _vm, bool _batch, SBatchOptions& _batchOptions)
{
    CCliHelper::conflictingOptions(_vm, "cmds", "cf");
    if (_batch) {
        if ((_vm.count("cmds") && _vm["cmds"].defaulted() && _vm.count("cf") && _vm["cf"].defaulted())
            || (_vm.count("cmds") && !_vm["cmds"].defaulted())) {
            _batchOptions.m_outputCmds = _batchOptions.m_cmds;
        } else if (_vm.count("cf") && !_vm["cf"].defaulted()) {
            _batchOptions.m_outputCmds = CCmdsFile::getCmds(_batchOptions.m_cmdsFilepath);
        }
    } else {
        _batchOptions.m_outputCmds = vector<string>();
    }
}

// Generic options

void CCliHelper::addHelpOptions(bpo::options_description& _options)
{
    _options.add_options()("help,h", "Produce help message");
}

void CCliHelper::addVersionOptions(bpo::options_description& _options)
{
    _options.add_options()("version,v", "Print version");
}

void CCliHelper::addSyncOptions(bpo::options_description& _options, bool& _sync)
{
    _options.add_options()("sync", bpo::bool_switch(&_sync)->default_value(false), "Use sync implementation of the gRPC server");
}

void CCliHelper::addTimeoutOptions(bpo::options_description& _options, size_t& _timeout)
{
    _options.add_options()("timeout", bpo::value<size_t>(&_timeout)->default_value(30), "Timeout of requests in sec");
}

void CCliHelper::addHostOptions(bpo::options_description& _options, string& _host)
{
    _options.add_options()("host", bpo::value<string>(&_host)->default_value("localhost:50051"), "Server address");
}

void CCliHelper::addLogOptions(bpo::options_description& _options, CLogger::SConfig& _config)
{
    string defaultLogDir{ smart_path(string("$HOME/.ODC/log")) };

    _options.add_options()("logdir", bpo::value<string>(&_config.m_logDir)->default_value(defaultLogDir), "Log files directory");
    _options.add_options()("severity", bpo::value<ESeverity>(&_config.m_severity)->default_value(ESeverity::info), "Log severity level");
    _options.add_options()("infologger", bpo::bool_switch(&_config.m_infologger)->default_value(false), "Enable InfoLogger (ODC needs to be compiled with InfoLogger support)");
}

void CCliHelper::addOptions(bpo::options_description& _options, SBatchOptions& _batchOptions)
{
    using boost::algorithm::join;

    const vector<partitionID_t> defaultPartitions{ "a1b2c3", "d4e5f6" };
    const string defaultUpTopo{ kODCDataDir + "/ex-dds-topology-infinite-up.xml" };
    const string defaultDownTopo{ kODCDataDir + "/ex-dds-topology-infinite-down.xml" };
    const string upStr{ ".upscale --topo " + defaultUpTopo };
    const string downStr{ ".downscale --topo " + defaultDownTopo };
    const vector<string> defaultCmds{ ".status", ".init",   ".status", ".submit", ".activate", ".config", ".start",
                                      ".status", ".stop",   upStr,     ".start",  ".status",   ".stop",   downStr,
                                      ".start",  ".status", ".stop",   ".reset",  ".term",     ".down",   ".status" };

    vector<string> cmds;
    for (const auto& cmd : defaultCmds) {
        // Give some time for processing before going to stop
        if (boost::starts_with(cmd, ".stop")) {
            cmds.push_back(".sleep --ms 5000");
        }
        if (boost::starts_with(cmd, ".status")) {
            cmds.push_back(cmd);
        } else {
            for (const auto& prt : defaultPartitions) {
                cmds.push_back(cmd + " --id " + prt);
            }
        }
    }

    const string cmdsStr{ join(cmds, " ") };
    _options.add_options()("cmds",
                           bpo::value<vector<string>>(&_batchOptions.m_cmds)->multitoken()->default_value(cmds, cmdsStr),
                           "Array of command to be executed in batch mode");

    const string defaultConfig{ kODCDataDir + "/cmds.cfg" };
    _options.add_options()("cf",
                           bpo::value<string>(&_batchOptions.m_cmdsFilepath)->default_value(defaultConfig),
                           "Config file containing an array of command to be executed in batch mode");
}

void CCliHelper::addBatchOptions(bpo::options_description& _options, SBatchOptions& _batchOptions, bool& _batch)
{
    _options.add_options()("batch", bpo::bool_switch(&_batch)->default_value(false), "Non interactive batch mode");

    CCliHelper::addOptions(_options, _batchOptions);
}

void CCliHelper::addOptions(bpo::options_description& _options, SSleepOptions& _sleepOptions)
{
    _options.add_options()("ms", bpo::value<size_t>(&_sleepOptions.m_ms)->default_value(1000), "Sleep time in ms");
}

void CCliHelper::addRestoreOptions(bpo::options_description& _options, string& _restoreId)
{
    _options.add_options()("restore",
                           bpo::value<string>(&_restoreId)->default_value(""),
                           "If set ODC will restore the sessions from file with specified ID");
}

//
// Request specific options
//

void CCliHelper::addOptions(bpo::options_description& _options, partitionID_t& _partitionID)
{
    _options.add_options()("id", bpo::value<partitionID_t>(&_partitionID)->default_value(""), "Partition ID");
}

void CCliHelper::addOptions(bpo::options_description& _options, SCommonParams& _common)
{
    _options.add_options()("id", bpo::value<partitionID_t>(&_common.m_partitionID)->default_value(""), "Partition ID");
    _options.add_options()("run", bpo::value<runNr_t>(&_common.m_runNr)->default_value(0), "Run Nr");
    _options.add_options()("timeout", bpo::value<size_t>(&_common.m_timeout)->default_value(0), "Request timeout");
}

void CCliHelper::addOptions(bpo::options_description& _options, SInitializeParams& _params)
{
    _options.add_options()("sid", bpo::value<string>(&_params.m_sessionID)->default_value(""), "Session ID of DDS");
}

void CCliHelper::addOptions(bpo::options_description& _options, SActivateParams& _params)
{
    string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite.xml");
    _options.add_options()("topo",
                           bpo::value<string>(&_params.m_topologyFile)->implicit_value("")->default_value(defaultTopo),
                           "Topology filepath");
    _options.add_options()(
        "content", bpo::value<string>(&_params.m_topologyContent)->implicit_value("")->default_value(""), "Topology content");
    _options.add_options()(
        "script", bpo::value<string>(&_params.m_topologyScript)->implicit_value("")->default_value(""), "Topology script");
}

void CCliHelper::addOptions(bpo::options_description& _options, SUpdateParams& _params)
{
    string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-up.xml");
    // string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-down.xml");
    _options.add_options()(
        "topo", bpo::value<string>(&_params.m_topologyFile)->default_value(defaultTopo), "Topology filepath");
    _options.add_options()("content", bpo::value<string>(&_params.m_topologyContent)->default_value(""), "Topology content");
    _options.add_options()(
        "script", bpo::value<string>(&_params.m_topologyScript)->implicit_value("")->default_value(""), "Topology script");
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

void CCliHelper::addOptions(bpo::options_description& _options, SDeviceParams& _params)
{
    _options.add_options()("path", bpo::value<string>(&_params.m_path)->default_value(""), "Topology path of devices");
    _options.add_options()("detailed", bpo::bool_switch(&_params.m_detailed)->default_value(false), "Detailed reply of devices");
}

void CCliHelper::addOptions(bpo::options_description& _options, SSetPropertiesParams& _params)
{
    _options.add_options()("path", bpo::value<string>(&_params.m_path)->default_value(""), "Path for a set property request");

    const SSetPropertiesParams::Properties_t props{ { "key1", "value1" }, { "key2", "value2" } };
    vector<string> defaults;
    transform(props.begin(), props.end(), back_inserter(defaults), [](const SSetPropertiesParams::Property_t& _p) -> string {
        return _p.first + ":" + _p.second;
    });
    string defaultsStr{ boost::algorithm::join(defaults, " ") };
    _options.add_options()("prop",
                           bpo::value<vector<string>>()->multitoken()->default_value(defaults, defaultsStr),
                           "Key-value pairs for a set properties request ( key1:value1 key2:value2 )");
}

void CCliHelper::addOptions(bpo::options_description& _options, SStatusParams& _params)
{
    _options.add_options()(
        "running", bpo::bool_switch(&_params.m_running)->default_value(false), "Select only running sessions");
}

//
// Resource plugins
//

void CCliHelper::addResourcePluginOptions(bpo::options_description& _options, CPluginManager::PluginMap_t& /*_pluginMap*/)
{
    _options.add_options()(
        "rp", bpo::value<vector<string>>()->multitoken(), "Register resource plugins ( name1:cmd1 name2:cmd2 )");
}

//
// Resource plugins
//

void CCliHelper::addRequestTriggersOptions(bpo::options_description& _options, CPluginManager::PluginMap_t& /*_pluginMap*/)
{
    _options.add_options()(
        "rt", bpo::value<vector<string>>()->multitoken(), "Register request triggers ( name1:cmd1 name2:cmd2 )");
}

//
// Extra step of options parsing
//

void CCliHelper::parseOptions(const bpo::variables_map& _vm, partitionID_t& /* _partitionID */, SSetPropertiesParams& _params)
{
    if (_vm.count("prop")) {
        const auto& kvp(_vm["prop"].as<vector<string>>());
        SSetPropertiesParams::Properties_t props;
        for (const auto& v : kvp) {
            vector<string> strs;
            boost::split(strs, v, boost::is_any_of(":"));
            if (strs.size() == 2) {
                props.push_back({ strs[0], strs[1] });
            } else {
                throw runtime_error("Wrong property format for string '" + v + "'. Use 'key:value'.");
            }
        }
        _params.m_properties = props;
    } else {
        _params.m_properties = { { "key1", "value1" }, { "key2", "value2" } };
    }
}

void CCliHelper::parsePluginMapOptions(const bpo::variables_map& _vm,
                                       CPluginManager::PluginMap_t& _pluginMap,
                                       const string& _option)
{
    if (_vm.count(_option)) {
        const auto& kvp(_vm[_option].as<vector<string>>());
        _pluginMap.clear();
        for (const auto& v : kvp) {
            const auto idx{ v.find_first_of(':') };
            if (string::npos != idx) {
                _pluginMap.insert(make_pair(v.substr(0, idx), v.substr(idx + 1)));
            } else {
                throw runtime_error(toString("Wrong plugin map format for string ", quoted(v), ". Use ", quoted("name:value")));
            }
        }
    } else {
        _pluginMap.clear();
    }
}

void CCliHelper::parseOptions(const bpo::variables_map& _vm, CCliHelper::SBatchOptions& _params)
{
    CCliHelper::batchCmds(_vm, true, _params);
}
