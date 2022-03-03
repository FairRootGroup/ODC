/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <odc/BuildConstants.h>
#include <odc/CliHelper.h>
#include <odc/CmdsFile.h>
// BOOST
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace odc::core;
namespace bpo = boost::program_options;

void CCliHelper::conflictingOptions(const bpo::variables_map& vm, const string& opt1, const string& opt2)
{
    if (vm.count(opt1) && !vm[opt1].defaulted() && vm.count(opt2) && !vm[opt2].defaulted()) {
        stringstream ss;
        ss << "Conflicting options " << quoted(opt1) << " and " << quoted(opt2);
        throw runtime_error(ss.str());
    }
}

void CCliHelper::batchCmds(const bpo::variables_map& vm, bool batch, SBatchOptions& batchOptions)
{
    CCliHelper::conflictingOptions(vm, "cmds", "cf");
    if (batch) {
        if ((vm.count("cmds") && vm["cmds"].defaulted() && vm.count("cf") && vm["cf"].defaulted()) || (vm.count("cmds") && !vm["cmds"].defaulted())) {
            batchOptions.m_outputCmds = batchOptions.m_cmds;
        } else if (vm.count("cf") && !vm["cf"].defaulted()) {
            batchOptions.m_outputCmds = CCmdsFile::getCmds(batchOptions.m_cmdsFilepath);
        }
    } else {
        batchOptions.m_outputCmds = vector<string>();
    }
}

// Generic options

void CCliHelper::addHelpOptions(bpo::options_description& options) { options.add_options()("help,h", "Produce help message"); }

void CCliHelper::addVersionOptions(bpo::options_description& options) { options.add_options()("version,v", "Print version"); }

void CCliHelper::addSyncOptions(bpo::options_description& options, bool& sync)
{
    options.add_options()("sync", bpo::bool_switch(&sync)->default_value(false), "Use sync implementation of the gRPC server");
}

void CCliHelper::addHostOptions(bpo::options_description& options, string& host)
{
    options.add_options()("host", bpo::value<string>(&host)->default_value("localhost:50051"), "Server address");
}

void CCliHelper::addLogOptions(bpo::options_description& options, CLogger::SConfig& config)
{
    string defaultLogDir{ smart_path(string("$HOME/.ODC/log")) };

    options.add_options()("logdir", bpo::value<string>(&config.m_logDir)->default_value(defaultLogDir), "Log files directory");
    options.add_options()("severity", bpo::value<ESeverity>(&config.m_severity)->default_value(ESeverity::info), "Log severity level");
    options.add_options()("infologger", bpo::bool_switch(&config.m_infologger)->default_value(false), "Enable InfoLogger (ODC needs to be compiled with InfoLogger support)");
}

void CCliHelper::addTimeoutOptions(bpo::options_description& options, size_t& timeout)
{
    options.add_options()("timeout", bpo::value<size_t>(&timeout)->default_value(30), "Timeout of requests in sec");
}

void CCliHelper::addOptions(bpo::options_description& options, SBatchOptions& batchOptions)
{
    const vector<partitionID_t> defaultPartitions{ "a1b2c3", "d4e5f6" };
    const string defaultUpTopo{ kODCDataDir + "/ex-dds-topology-infinite-up.xml" };
    const string defaultDownTopo{ kODCDataDir + "/ex-dds-topology-infinite-down.xml" };
    const string upStr{ ".upscale --topo " + defaultUpTopo };
    const string downStr{ ".downscale --topo " + defaultDownTopo };
    const vector<string> defaultCmds{ ".status", ".init", ".status", ".submit", ".activate", ".config", ".start", ".status", ".stop", upStr,    ".start",
                                      ".status", ".stop", downStr,   ".start",  ".status",   ".stop",   ".reset", ".term",   ".down", ".status" };

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

    const string cmdsStr{ boost::algorithm::join(cmds, " ") };
    options.add_options()("cmds", bpo::value<vector<string>>(&batchOptions.m_cmds)->multitoken()->default_value(cmds, cmdsStr), "Array of command to be executed in batch mode");

    const string defaultConfig{ kODCDataDir + "/cmds.cfg" };
    options.add_options()("cf", bpo::value<string>(&batchOptions.m_cmdsFilepath)->default_value(defaultConfig), "Config file containing an array of command to be executed in batch mode");
}

void CCliHelper::addBatchOptions(bpo::options_description& options, SBatchOptions& batchOptions, bool& batch)
{
    options.add_options()("batch", bpo::bool_switch(&batch)->default_value(false), "Non interactive batch mode");
    addOptions(options, batchOptions);
}

void CCliHelper::addOptions(bpo::options_description& options, SSleepOptions& sleepOptions)
{
    options.add_options()("ms", bpo::value<size_t>(&sleepOptions.m_ms)->default_value(1000), "Sleep time in ms");
}

void CCliHelper::addRestoreOptions(bpo::options_description& options, string& restoreId)
{
    options.add_options()("restore", bpo::value<string>(&restoreId)->default_value(""), "If set ODC will restore the sessions from file with specified ID");
}

// Request specific options

void CCliHelper::addOptions(bpo::options_description& options, partitionID_t& partitionID)
{
    options.add_options()("id", bpo::value<partitionID_t>(&partitionID)->default_value(""), "Partition ID");
}

void CCliHelper::addOptions(bpo::options_description& options, SCommonParams& common)
{
    options.add_options()("id", bpo::value<partitionID_t>(&common.m_partitionID)->default_value(""), "Partition ID");
    options.add_options()("run", bpo::value<runNr_t>(&common.m_runNr)->default_value(0), "Run Nr");
    options.add_options()("timeout", bpo::value<size_t>(&common.m_timeout)->default_value(0), "Request timeout");
}

void CCliHelper::addOptions(bpo::options_description& options, SInitializeParams& params)
{
    options.add_options()("sid", bpo::value<string>(&params.m_sessionID)->default_value(""), "Session ID of DDS");
}

void CCliHelper::addOptions(bpo::options_description& options, SActivateParams& params)
{
    string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite.xml");
    options.add_options()("topo", bpo::value<string>(&params.m_topologyFile)->implicit_value("")->default_value(defaultTopo), "Topology filepath");
    options.add_options()("content", bpo::value<string>(&params.m_topologyContent)->implicit_value("")->default_value(""), "Topology content");
    options.add_options()("script", bpo::value<string>(&params.m_topologyScript)->implicit_value("")->default_value(""), "Topology script");
}

void CCliHelper::addOptions(bpo::options_description& options, SUpdateParams& params)
{
    string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-up.xml");
    // string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-down.xml");
    options.add_options()("topo", bpo::value<string>(&params.m_topologyFile)->default_value(defaultTopo), "Topology filepath");
    options.add_options()("content", bpo::value<string>(&params.m_topologyContent)->default_value(""), "Topology content");
    options.add_options()("script", bpo::value<string>(&params.m_topologyScript)->implicit_value("")->default_value(""), "Topology script");
}

void CCliHelper::addOptions(bpo::options_description& options, SSubmitParams& params)
{
    options.add_options()("plugin,p", bpo::value<string>(&params.m_plugin)->default_value("odc-rp-same"), "ODC resource plugin name.");
    options.add_options()("resources,r",
                          bpo::value<string>(&params.m_resources)->default_value("<rms>localhost</rms><agents>1</agents><slots>36</slots><requiredSlots>36</requiredSlots>"),
                          "A resource description for a corresponding ODC resource plugin.");
}

void CCliHelper::addOptions(bpo::options_description& options, SDeviceParams& params)
{
    options.add_options()("path", bpo::value<string>(&params.m_path)->default_value(""), "Topology path of devices");
    options.add_options()("detailed", bpo::bool_switch(&params.m_detailed)->default_value(false), "Detailed reply of devices");
}

void CCliHelper::addOptions(bpo::options_description& options, SSetPropertiesParams& params)
{
    options.add_options()("path", bpo::value<string>(&params.m_path)->default_value(""), "Path for a set property request");

    const SSetPropertiesParams::Properties_t props{ { "key1", "value1" }, { "key2", "value2" } };
    vector<string> defaults;
    transform(props.begin(), props.end(), back_inserter(defaults), [](const SSetPropertiesParams::Property_t& p) -> string { return p.first + ":" + p.second; });
    string defaultsStr{ boost::algorithm::join(defaults, " ") };
    options.add_options()("prop", bpo::value<vector<string>>()->multitoken()->default_value(defaults, defaultsStr), "Key-value pairs for a set properties request ( key1:value1 key2:value2 )");
}

void CCliHelper::addOptions(bpo::options_description& options, SStatusParams& params)
{
    options.add_options()("running", bpo::bool_switch(&params.m_running)->default_value(false), "Select only running sessions");
}

void CCliHelper::addResourcePluginOptions(bpo::options_description& options, CPluginManager::PluginMap_t& /*_pluginMap*/)
{
    options.add_options()("rp", bpo::value<vector<string>>()->multitoken(), "Register resource plugins ( name1:cmd1 name2:cmd2 )");
}

void CCliHelper::addRequestTriggersOptions(bpo::options_description& options, CPluginManager::PluginMap_t& /*_pluginMap*/)
{
    options.add_options()("rt", bpo::value<vector<string>>()->multitoken(), "Register request triggers ( name1:cmd1 name2:cmd2 )");
}

// Extra step of options parsing

void CCliHelper::parseOptions(const bpo::variables_map& vm, partitionID_t& /* partitionID */, SSetPropertiesParams& params)
{
    if (vm.count("prop")) {
        const auto& kvp(vm["prop"].as<vector<string>>());
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
        params.m_properties = props;
    } else {
        params.m_properties = { { "key1", "value1" }, { "key2", "value2" } };
    }
}

void CCliHelper::parsePluginMapOptions(const bpo::variables_map& vm, CPluginManager::PluginMap_t& pluginMap, const string& option)
{
    if (vm.count(option)) {
        const auto& kvp(vm[option].as<vector<string>>());
        pluginMap.clear();
        for (const auto& v : kvp) {
            const auto idx{ v.find_first_of(':') };
            if (string::npos != idx) {
                pluginMap.insert(make_pair(v.substr(0, idx), v.substr(idx + 1)));
            } else {
                throw runtime_error(toString("Wrong plugin map format for string ", quoted(v), ". Use ", quoted("name:value")));
            }
        }
    } else {
        pluginMap.clear();
    }
}

void CCliHelper::parseOptions(const bpo::variables_map& vm, CCliHelper::SBatchOptions& params) { CCliHelper::batchCmds(vm, true, params); }
