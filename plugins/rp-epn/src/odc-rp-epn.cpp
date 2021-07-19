// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// STD
#include <iostream>
// BOOST
#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/json_parser.hpp>
// DDS
#include "dds/SSHConfigFile.h"
// ODC
#include "CliHelper.h"
#include "EpncClient.h"
#include "Logger.h"

using namespace std;
namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;
using namespace odc;
using namespace odc::core;

struct SResources
{
    SResources(const string& _res)
    {
        parse(_res);
    }

    string m_zone;
    size_t m_n{ 0 };

  private:
    void parse(const string& _res)
    {
        boost::property_tree::ptree pt;
        stringstream ss;
        ss << _res;
        boost::property_tree::read_json(ss, pt);

        // Do some basic validation of the input tree.
        // Only valid tags are allowed.
        set<string> validTags{ "zone", "n" };
        for (const auto& v : pt)
        {
            if (validTags.count(v.first.data()) == 0)
            {
                stringstream ss;
                ss << "Failed to init from property tree. Unknown key " << quoted(v.first.data());
                throw runtime_error(ss.str());
            }
        }
        m_zone = pt.get<string>("zone", "online");
        m_n = pt.get<size_t>("n", 1);
    }
};

int main(int argc, char** argv)
{
    try
    {
        string res;
        string host;
        bool release;
        CLogger::SConfig logConfig;
        partitionID_t partitionID;

        // Generic options
        bpo::options_description options("odc-rp-epn options");
        CCliHelper::addHelpOptions(options);
        CCliHelper::addVersionOptions(options);
        CCliHelper::addLogOptions(options, logConfig);
        CCliHelper::addOptions(options, partitionID);
        options.add_options()("res",
                              bpo::value<string>(&res)->default_value("{\"zone\":\"online\",\"n\":10}"),
                              "Resource description in JSON format");
        options.add_options()(
            "host", bpo::value<string>(&host)->default_value("localhost:50001"), "EPN controller endpoint");
        options.add_options()(
            "release", bpo::bool_switch(&release)->default_value(false), "If set then release allocation");

        // Parsing command-line
        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(argc, argv).options(options).run(), vm);
        bpo::notify(vm);

        try
        {
            CLogger::instance().init(logConfig);
        }
        catch (exception& _e)
        {
            cerr << "Can't initialize log: " << _e.what() << endl;
            return EXIT_FAILURE;
        }

        if (vm.count("help"))
        {
            cout << options;
            return EXIT_SUCCESS;
        }

        OLOG(ESeverity::info) << "Start epnc plugin";
        CEpncClient client(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
        if (!release)
        {
            auto r{ SResources(res) };
            vector<string> nodes;
            client.allocateNodes(partitionID, r.m_zone, r.m_n, nodes);
            OLOG(ESeverity::info) << "epnc: nodes requested (" << r.m_n << ") allocated (" << nodes.size() << ")"
                                  << endl;

            // Create SSH config file
            // TODO: make hardcoded values as configuration parameters of the plugin
            const bfs::path tmpPath{ bfs::temp_directory_path() / bfs::unique_path() };
            bfs::create_directories(tmpPath);
            const bfs::path filepath{ tmpPath / "hosts.cfg" };
            string sshOptions{ "" };
            string wrkDir{ "/tmp/wn_dds" };
            size_t numSlots{ 128 };
            string bash{ "module load DataDistribution QualityControl" };
            dds::CSSHConfigFile::make(filepath.string(), nodes, sshOptions, wrkDir, numSlots, bash);

            OLOG(ESeverity::info) << "epnc: SSH config file created at path " << filepath << endl;

            stringstream ss;
            int requiredSlots(nodes.size() * numSlots);
            ss << "<rms>ssh</rms><configFile>" << filepath.string() << "</configFile><requiredSlots>" << requiredSlots
               << "</requiredSlots>";

            OLOG(ESeverity::info) << ss.str();
            OLOG(ESeverity::clean) << ss.str();
        }
        else
        {
            client.releasePartition(partitionID);
        }
        OLOG(ESeverity::info) << "Finish epnc plugin";
    }
    catch (exception& _e)
    {
        cerr << _e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
