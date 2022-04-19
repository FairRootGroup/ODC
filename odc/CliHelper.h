/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__CliHelper__
#define __ODC__CliHelper__

#include <odc/BuildConstants.h>
#include <odc/CmdsFile.h>
#include <odc/Logger.h>
#include <odc/Params.h>
#include <odc/PluginManager.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace odc::core
{

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

    /// Checks that 'opt1' and 'opt2' are not specified at the same time
    static void conflictingOptions(const boost::program_options::variables_map& vm, const std::string& opt1, const std::string& opt2)
    {
        if (vm.count(opt1) && !vm[opt1].defaulted() && vm.count(opt2) && !vm[opt2].defaulted()) {
            std::stringstream ss;
            ss << "Conflicting options " << std::quoted(opt1) << " and " << std::quoted(opt2);
            throw std::runtime_error(ss.str());
        }
    }

    /// \brief Fills BatchOptions::m_outputCmds
    static void batchCmds(const boost::program_options::variables_map& vm, bool batch, BatchOptions& batchOptions)
    {
        CliHelper::conflictingOptions(vm, "cmds", "cf");
        if (batch) {
            if ((vm.count("cmds") && vm["cmds"].defaulted() && vm.count("cf") && vm["cf"].defaulted()) || (vm.count("cmds") && !vm["cmds"].defaulted())) {
                batchOptions.mOutputCmds = batchOptions.mCmds;
            } else if (vm.count("cf") && !vm["cf"].defaulted()) {
                batchOptions.mOutputCmds = CmdsFile::getCmds(batchOptions.mCmdsFilepath);
            }
        } else {
            batchOptions.mOutputCmds = std::vector<std::string>();
        }
    }

    // Generic options

    static void addLogOptions(boost::program_options::options_description& options, Logger::Config& config)
    {
        using namespace boost::program_options;
        std::string defaultLogDir{ smart_path(std::string("$HOME/.ODC/log")) };
        options.add_options()
            ("logdir", value<std::string>(&config.mLogDir)->default_value(defaultLogDir), "Log files directory")
            ("severity", value<ESeverity>(&config.mSeverity)->default_value(ESeverity::info), "Log severity level")
            ("infologger", bool_switch(&config.mInfologger)->default_value(false), "Enable InfoLogger (ODC needs to be compiled with InfoLogger support)")
            ("infologger-system", value<std::string>(&config.mInfologgerSystem)->default_value("ODC"), "Fills the InfoLogger 'System' field")
            ("infologger-facility", value<std::string>(&config.mInfologgerFacility)->default_value("ODC"), "Fills the InfoLogger 'Facility' field")
            ("infologger-role", value<std::string>(&config.mInfologgerRole)->default_value("production"), "Fills the InfoLogger 'Role' field");
    }

    static void addOptions(boost::program_options::options_description& options, BatchOptions& batchOptions)
    {
        const std::vector<std::string> defaultPartitions{ "a1b2c3", "d4e5f6" };
        const std::string defaultUpTopo{ kODCDataDir + "/ex-dds-topology-infinite-up.xml" };
        const std::string defaultDownTopo{ kODCDataDir + "/ex-dds-topology-infinite-down.xml" };
        const std::string upStr{ ".upscale --topo " + defaultUpTopo };
        const std::string downStr{ ".downscale --topo " + defaultDownTopo };
        const std::vector<std::string> defaultCmds{ ".status", ".init", ".status", ".submit", ".activate", ".config", ".start", ".status", ".stop", upStr,    ".start",
                                                    ".status", ".stop", downStr,   ".start",  ".status",   ".stop",   ".reset", ".term",   ".down", ".status" };

        std::vector<std::string> cmds;
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

        const std::string cmdsStr{ boost::algorithm::join(cmds, " ") };
        options.add_options()
            ("cmds", boost::program_options::value<std::vector<std::string>>(&batchOptions.mCmds)->multitoken()->default_value(cmds, cmdsStr),
                "Array of command to be executed in batch mode");

        const std::string defaultConfig{ kODCDataDir + "/ex-cmds.cfg" };
        options.add_options()
            ("cf", boost::program_options::value<std::string>(&batchOptions.mCmdsFilepath)->default_value(defaultConfig),
                "Config file containing an array of command to be executed in batch mode");
    }

    static void addBatchOptions(boost::program_options::options_description& options, BatchOptions& batchOptions, bool& batch)
    {
        options.add_options()("batch", boost::program_options::bool_switch(&batch)->default_value(false), "Non interactive batch mode");
        addOptions(options, batchOptions);
    }

    static void addOptions(boost::program_options::options_description& options, SleepOptions& sleepOptions)
    {
        options.add_options()("ms", boost::program_options::value<size_t>(&sleepOptions.mMs)->default_value(1000), "Sleep time in ms");
    }

    // Request specific options

    static void addOptions(boost::program_options::options_description& options, CommonParams& common)
    {
        using namespace boost::program_options;
        options.add_options()
            ("id", value<std::string>(&common.mPartitionID)->default_value(""), "Partition ID")
            ("run", value<uint64_t>(&common.mRunNr)->default_value(0), "Run Nr")
            ("timeout", value<size_t>(&common.mTimeout)->default_value(0), "Request timeout");
    }

    static void addOptions(boost::program_options::options_description& options, InitializeParams& params)
    {
        options.add_options()("sid", boost::program_options::value<std::string>(&params.mDDSSessionID)->default_value(""), "Session ID of DDS");
    }

    static void addOptions(boost::program_options::options_description& options, ActivateParams& params)
    {
        using namespace boost::program_options;
        std::string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite.xml");
        options.add_options()
            ("topo", value<std::string>(&params.mDDSTopologyFile)->implicit_value("")->default_value(defaultTopo), "Topology filepath")
            ("content", value<std::string>(&params.mDDSTopologyContent)->implicit_value("")->default_value(""), "Topology content")
            ("script", value<std::string>(&params.mDDSTopologyScript)->implicit_value("")->default_value(""), "Topology script");
    }

    static void addOptions(boost::program_options::options_description& options, UpdateParams& params)
    {
        using namespace boost::program_options;
        std::string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-up.xml");
        // std::string defaultTopo(kODCDataDir + "/ex-dds-topology-infinite-down.xml");
        options.add_options()
            ("topo", value<std::string>(&params.mDDSTopologyFile)->default_value(defaultTopo), "Topology filepath")
            ("content", value<std::string>(&params.mDDSTopologyContent)->default_value(""), "Topology content")
            ("script", value<std::string>(&params.mDDSTopologyScript)->implicit_value("")->default_value(""), "Topology script");
    }

    static void addOptions(boost::program_options::options_description& options, SubmitParams& params)
    {
        using namespace boost::program_options;
        std::string defaultTopo("<rms>localhost</rms><agents>1</agents><slots>36</slots><requiredSlots>36</requiredSlots>");
        options.add_options()
            ("plugin,p", value<std::string>(&params.mPlugin)->default_value("odc-rp-same"), "ODC resource plugin name.")
            ("resources,r", value<std::string>(&params.mResources)->default_value(defaultTopo), "A resource description for a corresponding ODC resource plugin.");
    }

    static void addOptions(boost::program_options::options_description& options, DeviceParams& params)
    {
        options.add_options()
            ("path", boost::program_options::value<std::string>(&params.mPath)->default_value(""), "Topology path of devices")
            ("detailed", boost::program_options::bool_switch(&params.mDetailed)->default_value(false), "Detailed reply of devices");
    }

    static void addOptions(boost::program_options::options_description& options, SetPropertiesParams& params)
    {
        using namespace boost::program_options;
        const SetPropertiesParams::Properties_t props{ { "key1", "value1" }, { "key2", "value2" } };
        std::vector<std::string> defaults;
        transform(props.begin(), props.end(), back_inserter(defaults), [](const SetPropertiesParams::Property_t& p) -> std::string { return p.first + ":" + p.second; });
        std::string defaultsStr{ boost::algorithm::join(defaults, " ") };
        options.add_options()
            ("prop", value<std::vector<std::string>>()->multitoken()->default_value(defaults, defaultsStr),
                "Key-value pairs for a set properties request ( key1:value1 key2:value2 )")
            ("path", value<std::string>(&params.mPath)->default_value(""), "Path for a set property request");
    }

    static void addOptions(boost::program_options::options_description& options, StatusParams& params)
    {
        options.add_options()("running", boost::program_options::bool_switch(&params.mRunning)->default_value(false), "Select only running sessions");
    }

    // Options parsing

    static void parsePluginMapOptions(const boost::program_options::variables_map& vm, CPluginManager::PluginMap_t& pluginMap, const std::string& option)
    {
        if (vm.count(option)) {
            const auto& kvp(vm[option].as<std::vector<std::string>>());
            pluginMap.clear();
            for (const auto& v : kvp) {
                const auto idx = v.find_first_of(':');
                if (idx != std::string::npos) {
                    pluginMap.insert(std::make_pair(v.substr(0, idx), v.substr(idx + 1)));
                } else {
                    throw std::runtime_error(toString("Wrong plugin map format for string ", std::quoted(v), ". Use ", std::quoted("name:value")));
                }
            }
        } else {
            pluginMap.clear();
        }
    }

    template<typename... RequestParams_t>
    static void parseOptions(const boost::program_options::variables_map& /*vm*/, RequestParams_t&&... /*params*/)
    {} // Default implementation does nothing

    static void parseOptions(const boost::program_options::variables_map& vm, std::string& /* partitionID */, SetPropertiesParams& params)
    {
        if (vm.count("prop")) {
            const auto& kvp(vm["prop"].as<std::vector<std::string>>());
            SetPropertiesParams::Properties_t props;
            for (const auto& v : kvp) {
                std::vector<std::string> strs;
                boost::split(strs, v, boost::is_any_of(":"));
                if (strs.size() == 2) {
                    props.push_back({ strs[0], strs[1] });
                } else {
                    throw std::runtime_error("Wrong property format for string '" + v + "'. Use 'key:value'.");
                }
            }
            params.mProperties = props;
        } else {
            params.mProperties = { { "key1", "value1" }, { "key2", "value2" } };
        }
    }

    static void parseOptions(const boost::program_options::variables_map& vm, CliHelper::BatchOptions& params) { CliHelper::batchCmds(vm, true, params); }
};

} // namespace odc::core

#endif /* defined(__ODC__CliHelper__) */
