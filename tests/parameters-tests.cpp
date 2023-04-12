/********************************************************************************
 * Copyright (C) 2019-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#define BOOST_TEST_MODULE(odc_parameters)
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include <odc/BuildConstants.h>
#include <odc/Controller.h>
#include <odc/DDSSubmit.h>
#include <odc/MiscUtils.h>
#include <odc/Session.h>
#include <odc/TopologyDefs.h>

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace boost::unit_test;

using namespace odc::core;
using namespace fair::mq;

using std::chrono::seconds;
using std::map;
using std::string;
using std::vector;

void printParams(const vector<DDSSubmitParams>& ddsParams)
{
    std::cout << "Parameter container with " << ddsParams.size() << " parameter set(s):" << std::endl;
    for (const auto& p : ddsParams) {
        std::cout << "  " << p << std::endl;
    }
}

void testParameterSet(
    DDSSubmitParams& set,
    const string& rms,
    const string& zone,
    const string& agentGroup,
    int32_t numAgents,
    size_t minAgents,
    size_t numSlots,
    size_t numCores,
    const string& configFile,
    const string& envFile)
{
    BOOST_TEST(set.mRMS == rms);
    BOOST_TEST(set.mZone == zone);
    BOOST_TEST(set.mAgentGroup == agentGroup);
    BOOST_TEST(set.mNumAgents == numAgents);
    BOOST_TEST(set.mMinAgents == minAgents);
    BOOST_TEST(set.mNumSlots == numSlots);
    BOOST_TEST(set.mNumCores == numCores);
    BOOST_TEST(set.mConfigFile == configFile);
    BOOST_TEST(set.mEnvFile == envFile);
}

void compareParameterSets(const DDSSubmitParams& set1, const DDSSubmitParams& set2)
{
    BOOST_TEST(set1.mRMS == set2.mRMS);
    BOOST_TEST(set1.mZone == set2.mZone);
    BOOST_TEST(set1.mAgentGroup == set2.mAgentGroup);
    BOOST_TEST(set1.mNumAgents == set2.mNumAgents);
    BOOST_TEST(set1.mMinAgents == set2.mMinAgents);
    BOOST_TEST(set1.mNumSlots == set2.mNumSlots);
    BOOST_TEST(set1.mNumCores == set2.mNumCores);
    BOOST_TEST(set1.mConfigFile == set2.mConfigFile);
    BOOST_TEST(set1.mEnvFile == set2.mEnvFile);
}

BOOST_AUTO_TEST_SUITE(creation)

BOOST_AUTO_TEST_CASE(odc_rp_same_simple)
{
    string rms = "localhost";
    map<string, ZoneConfig> zones;
    string plugin = "odc-rp-same";
    string resources = "<rms>localhost</rms><agents>1</agents><slots>36</slots>";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);

    Session session;
    session.mPartitionID = partitionId;
    session.mTopoFilePath = kODCDataDir + "/ex-topo-infinite-up.xml";
    Controller::extractRequirements(common, session);

    DDSSubmit ddsSubmit;
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, session.mZoneInfo, session.mNinfo, seconds(10));

    vector<DDSSubmitParams> ddsParams2;
    ddsParams2 = ddsSubmit.makeParams(rms, zones, session.mAgentGroupInfo);

    printParams(ddsParams);
    printParams(ddsParams2);

    BOOST_TEST(ddsParams.size() == 1);
    testParameterSet(ddsParams.at(0), "localhost", "", "", 1, 0, 36, 0, "", "");
    testParameterSet(ddsParams2.at(0), "localhost", "", "", 1, 0, 12, 0, "", "");
    testParameterSet(ddsParams2.at(1), "localhost", "", "", 1, 0, 12, 0, "", "");
    testParameterSet(ddsParams2.at(2), "localhost", "", "", 1, 0, 12, 0, "", "");
}

BOOST_AUTO_TEST_CASE(odc_rp_same_zones)
{
    string rms = "localhost";
    map<string, ZoneConfig> zones;
    string plugin = "odc-rp-same";
    // DDS localhost rms requires multiple submissions if the number of agents must be larger than 1
    string resources = "<submit><rms>localhost</rms><agents>1</agents><zone>calib</zone><slots>2</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>1</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>1</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>1</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>1</slots></submit>";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);

    Session session;
    session.mPartitionID = partitionId;
    session.mTopoFilePath = kODCDataDir + "/ex-topo-groupname.xml";
    Controller::extractRequirements(common, session);

    DDSSubmit ddsSubmit;
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, session.mZoneInfo, session.mNinfo, seconds(10));
    vector<DDSSubmitParams> ddsParams2;
    ddsParams2 = ddsSubmit.makeParams(rms, zones, session.mAgentGroupInfo);

    printParams(ddsParams);
    printParams(ddsParams2);

    BOOST_TEST(ddsParams.size() == 5);
    testParameterSet(ddsParams.at(0), "localhost", "calib", "calib", 1, 0, 2, 0, "", "");
    testParameterSet(ddsParams.at(1), "localhost", "online", "online", 1, 0, 1, 0, "", "");
    testParameterSet(ddsParams.at(2), "localhost", "online", "online", 1, 0, 1, 0, "", "");
    testParameterSet(ddsParams.at(3), "localhost", "online", "online", 1, 0, 1, 0, "", "");
    testParameterSet(ddsParams.at(4), "localhost", "online", "online", 1, 0, 1, 0, "", "");
}

BOOST_AUTO_TEST_CASE(odc_rp_epn_slurm_zones)
{
    string rms = "slurm";
    map<string, ZoneConfig> zones = {
        { "calib", { "/home/user/slurm-calib.cfg", "" } },
        { "online", { "/home/user/slurm-online.cfg", "" } }
    };
    string plugin = "epn";
    string resources = "[{ \"zone\":\"online\", \"n\":4 }, { \"zone\":\"calib\", \"n\":1 }]";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);

    Session session;
    session.mPartitionID = partitionId;
    session.mTopoFilePath = kODCDataDir + "/ex-topo-groupname-crashing.xml";
    Controller::extractRequirements(common, session);

    DDSSubmit ddsSubmit;
    vector<DDSSubmitParams> ddsParams;
    ddsSubmit.registerPlugin("epn", kODCBinDir + "/odc-rp-epn-slurm --zones online:2:/home/user/slurm-online.cfg: calib:2:/home/user/slurm-calib.cfg:");
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, session.mZoneInfo, session.mNinfo, seconds(10));
    vector<DDSSubmitParams> ddsParams2;
    ddsParams2 = ddsSubmit.makeParams(rms, zones, session.mAgentGroupInfo);

    printParams(ddsParams);
    printParams(ddsParams2);

    BOOST_TEST(ddsParams.size() == 2);
    BOOST_TEST(ddsParams2.size() == 2);
    testParameterSet(ddsParams.at(1), "slurm", "calib", "calib", 1, 0, 2, 0, "/home/user/slurm-calib.cfg", "");
    testParameterSet(ddsParams.at(0), "slurm", "online", "online", 4, 2, 2, 0, "/home/user/slurm-online.cfg", "");
    compareParameterSets(ddsParams.at(0), ddsParams2.at(0));
    compareParameterSets(ddsParams.at(1), ddsParams2.at(1));
}

BOOST_AUTO_TEST_CASE(odc_rp_epn_slurm_ncores)
{
    string rms = "slurm";
    map<string, ZoneConfig> zones = {
        { "calib", { "/home/user/slurm-calib.cfg", "" } },
        { "online", { "/home/user/slurm-online.cfg", "" } }
    };
    string plugin = "epn";
    string resources = "[{ \"zone\":\"calib\", \"n\":1 }, { \"zone\":\"online\", \"n\":4 }]";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);

    Session session;
    session.mPartitionID = partitionId;
    session.mTopoFilePath = kODCDataDir + "/ex-topo-groupname-ncores.xml";
    Controller::extractRequirements(common, session);

    DDSSubmit ddsSubmit;
    ddsSubmit.registerPlugin("epn", kODCBinDir + "/odc-rp-epn-slurm --zones online:1:/home/user/slurm-online.cfg: calib:1:/home/user/slurm-calib.cfg:");
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, session.mZoneInfo, session.mNinfo, seconds(10));
    vector<DDSSubmitParams> ddsParams2;
    ddsParams2 = ddsSubmit.makeParams(rms, zones, session.mAgentGroupInfo);

    printParams(ddsParams);
    printParams(ddsParams2);

    BOOST_TEST(ddsParams.size() == 3);
    BOOST_TEST(ddsParams2.size() == 3);
    testParameterSet(ddsParams.at(0), "slurm", "calib", "calib1", 1, 0, 1, 2, "/home/user/slurm-calib.cfg", "");
    testParameterSet(ddsParams.at(1), "slurm", "online", "online", 4, 0, 1, 0, "/home/user/slurm-online.cfg", "");
    testParameterSet(ddsParams.at(2), "slurm", "calib", "calib2", 1, 0, 1, 1, "/home/user/slurm-calib.cfg", "");
    compareParameterSets(ddsParams.at(0), ddsParams2.at(2));
    compareParameterSets(ddsParams.at(1), ddsParams2.at(0));
    compareParameterSets(ddsParams.at(2), ddsParams2.at(1));
}

BOOST_AUTO_TEST_CASE(odc_extract_epn)
{
    string rms = "slurm";
    map<string, ZoneConfig> zones = {
        { "calib", { "/home/user/slurm-calib.cfg", "" } },
        { "online", { "/home/user/slurm-online.cfg", "" } }
    };
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);

    Session session;
    session.mPartitionID = partitionId;
    session.mTopoFilePath = kODCDataDir + "/ex-epn.xml";
    Controller::extractRequirements(common, session);

    DDSSubmit ddsSubmit;
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(rms, zones, session.mAgentGroupInfo);

    printParams(ddsParams);

    BOOST_TEST(ddsParams.size() == 2);
    testParameterSet(ddsParams.at(0), "slurm", "online", "online", 50, 50, 223, 0, "/home/user/slurm-online.cfg", "");
    testParameterSet(ddsParams.at(1), "slurm", "calib", "calib1", 1, 0, 17, 128, "/home/user/slurm-calib.cfg", "");
}

BOOST_AUTO_TEST_SUITE_END()

int main(int argc, char* argv[]) { return boost::unit_test::unit_test_main(init_unit_test, argc, argv); }
