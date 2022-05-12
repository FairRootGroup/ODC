/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// STD
#include <iostream>
// BOOST
#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/json_parser.hpp>
// DDS
#include <dds/SSHConfigFile.h>
// ODC
#include <odc/CliHelper.h>
#include <EpncClient.h>
#include <odc/Logger.h>
#include <odc/Version.h>

using namespace std;
namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;
namespace bpt = boost::property_tree;
using namespace odc;
using namespace odc::core;

struct SResource
{
    SResource(const bpt::ptree& _pt)
    {
        // Do some basic validation of the input tree.
        // Only valid tags are allowed.
        set<string> validTags{ "zone", "n" };
        for (const auto& v : _pt) {
            if (validTags.count(v.first.data()) == 0) {
                stringstream ss;
                ss << "Failed to init from property tree. Unknown key " << quoted(v.first.data());
                throw runtime_error(ss.str());
            }
        }
        m_zone = _pt.get<string>("zone", "online");
        m_n = _pt.get<size_t>("n", 1);
    }

    string m_zone;
    size_t m_n{ 0 };
};

struct SResources
{
    SResources(const string& _res)
    {
        bpt::ptree pt;
        stringstream ss;
        ss << _res;
        bpt::read_json(ss, pt);

        try {
            // Single resource
            m_resources.push_back(SResource(pt));
        } catch (const exception& _e) {
            // Array of resources
            auto rpt{ pt.get_child_optional("") };
            if (rpt) {
                for (const auto& v : rpt.get()) {
                    m_resources.push_back(SResource(v.second));
                }
            }
        }
    }

    vector<SResource> m_resources;
};

int main(int argc, char** argv)
{
    try {
        string res;
        string host;
        bool release;
        Logger::Config logConfig;
        std::string partitionID;
        string bash;
        size_t numSlots{ 1 };
        string wrkDir;
        string sshopt;

        bpo::options_description opts("odc-rp-epn options");
        opts.add_options()("help,h", "Print help");
        opts.add_options()("version,v", "Print version");
        CliHelper::addLogOptions(opts, logConfig);
        opts.add_options()("id", boost::program_options::value<std::string>(&partitionID)->default_value(""), "Partition ID");
        opts.add_options()("res", bpo::value<string>(&res)->default_value("{\"zone\":\"online\",\"n\":1}"), "Resource description in JSON format");
        opts.add_options()("host", bpo::value<string>(&host)->default_value("localhost:50001"), "EPN controller endpoint");
        opts.add_options()("release", bpo::bool_switch(&release)->default_value(false), "If set then release allocation");
        opts.add_options()("bash", bpo::value<string>(&bash)->default_value(""), "Optional bash commands to be prepended to SSH config file");
        opts.add_options()("slots", bpo::value<size_t>(&numSlots)->default_value(1), "Number of slots for each worker node");
        opts.add_options()("wrkdir", bpo::value<string>(&wrkDir)->default_value("/tmp/wn_dds"), "Working directory");
        opts.add_options()("sshopt", bpo::value<string>(&sshopt)->default_value(""), "Additional SSH options");

        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(opts).run(), vm);
        bpo::notify(vm);

        try {
            Logger::instance().init(logConfig);
        } catch (exception& _e) {
            cerr << "Can't initialize log: " << _e.what() << endl;
            return EXIT_FAILURE;
        }

        if (vm.count("help")) {
            cout << opts;
            return EXIT_SUCCESS;
        }

        if (vm.count("version")) {
            OLOG(clean) << ODC_VERSION;
            return EXIT_SUCCESS;
        }

        OLOG(info, partitionID, 0) << "Start epnc plugin";
        CEpncClient client(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
        if (!release) {
            auto r{ SResources(res) };
            dds::configRecords_t records;

            for (const auto& v : r.m_resources) {
                vector<string> nodes;
                client.allocateNodes(partitionID, v.m_zone, v.m_n, nodes);
                OLOG(info, partitionID, 0) << "epnc: nodes requested (" << v.m_n << ") allocated (" << nodes.size() << ")" << endl;

                for (const auto& node : nodes) {
                    dds::configRecord_t record{ make_shared<dds::SConfigRecord>() };
                    record->m_id = "wn_" + v.m_zone + "_" + node;
                    record->m_addr = node;
                    record->m_sshOptions = sshopt;
                    record->m_wrkDir = wrkDir;
                    record->m_nSlots = numSlots;
                    records.push_back(record);
                }
            }

            // Create SSH config file
            const bfs::path tmpPath{ bfs::temp_directory_path() / bfs::unique_path() };
            bfs::create_directories(tmpPath);
            const bfs::path filepath{ tmpPath / "hosts.cfg" };
            dds::CSSHConfigFile::make(filepath.string(), records, bash);

            OLOG(info, partitionID, 0) << "epnc: SSH config file created at path " << filepath << endl;

            stringstream ss;
            ss << "<rms>ssh</rms><configFile>" << filepath.string() << "</configFile>";

            OLOG(info, partitionID, 0) << ss.str();
            OLOG(clean, partitionID, 0) << ss.str();
        } else {
            client.releasePartition(partitionID);
        }
        OLOG(info, partitionID, 0) << "Finish epnc plugin";
    } catch (exception& _e) {
        cerr << _e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
