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
#include <odc/DDSSubmit.h>
#include <odc/MiscUtils.h>
#include <odc/TopologyDefs.h>

#include <chrono>
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

void printParams(const vector<DDSSubmitParams>& ddsParams)
{
    OLOG(info) << "Parameter container with " << ddsParams.size() << " parameter set(s):";
    for (const auto& p : ddsParams) {
        OLOG(info) << "  " << p;
    }
}

BOOST_AUTO_TEST_SUITE(creation)

BOOST_AUTO_TEST_CASE(odc_rp_same_simple)
{
    string plugin = "odc-rp-same";
    string resources = "<rms>localhost</rms><agents>1</agents><slots>36</slots>";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    DDSSubmit ddsSubmit;
    map<string, vector<ZoneGroup>> zoneInfo;
    std::map<std::string, CollectionNInfo> nInfo;
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, zoneInfo, nInfo, seconds(10));

    printParams(ddsParams);

    BOOST_TEST(ddsParams.size() == 1);
    testParameterSet(ddsParams.at(0), "localhost", "", "", 1, 0, 36, 0, "", "");
}

BOOST_AUTO_TEST_CASE(odc_rp_same_zones)
{
    string plugin = "odc-rp-same";
    // DDS localhost rms requires multiple submissions if the number of agents must be larger than 1
    string resources = "<submit><rms>localhost</rms><agents>1</agents><zone>calib</zone><slots>2</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>2</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>2</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>2</slots></submit>"
                       "<submit><rms>localhost</rms><agents>1</agents><zone>online</zone><slots>2</slots></submit>";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    DDSSubmit ddsSubmit;
    map<string, vector<ZoneGroup>> zoneInfo;
    zoneInfo["calib"].emplace_back(ZoneGroup{1, 0, "calib"});
    zoneInfo["online"].emplace_back(ZoneGroup{4, 0, "online"});
    std::map<std::string, CollectionNInfo> nInfo;
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, zoneInfo, nInfo, seconds(10));

    printParams(ddsParams);

    BOOST_TEST(ddsParams.size() == 5);
    testParameterSet(ddsParams.at(0), "localhost", "calib", "calib", 1, 0, 2, 0, "", "");
    testParameterSet(ddsParams.at(1), "localhost", "online", "online", 1, 0, 2, 0, "", "");
    testParameterSet(ddsParams.at(2), "localhost", "online", "online", 1, 0, 2, 0, "", "");
    testParameterSet(ddsParams.at(3), "localhost", "online", "online", 1, 0, 2, 0, "", "");
    testParameterSet(ddsParams.at(4), "localhost", "online", "online", 1, 0, 2, 0, "", "");
}

BOOST_AUTO_TEST_CASE(odc_rp_epn_slurm_zones)
{
    string plugin = "epn";
    string resources = "[{ \"zone\":\"calib\", \"n\":1 }, { \"zone\":\"online\", \"n\":4 }]";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    DDSSubmit ddsSubmit;
    ddsSubmit.registerPlugin("epn", kODCBinDir + "/odc-rp-epn-slurm --zones calib:2:/home/user/slurm-calib.cfg: online:2:/home/user/slurm-online.cfg:");
    map<string, vector<ZoneGroup>> zoneInfo;
    zoneInfo["calib"].emplace_back(ZoneGroup{1, 0, "calib"});
    zoneInfo["online"].emplace_back(ZoneGroup{4, 0, "online"});
    std::map<std::string, CollectionNInfo> nInfo;
    nInfo.try_emplace("Processors", CollectionNInfo{ 4, 4, 2, "online" });
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, zoneInfo, nInfo, seconds(10));

    printParams(ddsParams);

    BOOST_TEST(ddsParams.size() == 2);
    testParameterSet(ddsParams.at(0), "slurm", "calib", "calib", 1, 0, 2, 0, "/home/user/slurm-calib.cfg", "");
    testParameterSet(ddsParams.at(1), "slurm", "online", "online", 4, 2, 2, 0, "/home/user/slurm-online.cfg", "");
}

BOOST_AUTO_TEST_CASE(odc_rp_epn_slurm_ncores)
{
    string plugin = "epn";
    string resources = "[{ \"zone\":\"calib\", \"n\":1 }, { \"zone\":\"online\", \"n\":1 }]";
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    DDSSubmit ddsSubmit;
    ddsSubmit.registerPlugin("epn", kODCBinDir + "/odc-rp-epn-slurm --zones calib:1:/home/user/slurm-calib.cfg: online:1:/home/user/slurm-online.cfg:");
    map<string, vector<ZoneGroup>> zoneInfo;
    zoneInfo["calib"].emplace_back(ZoneGroup{1, 2, "calib1"});
    zoneInfo["calib"].emplace_back(ZoneGroup{1, 1, "calib2"});
    zoneInfo["online"].emplace_back(ZoneGroup{4, 0, "online"});
    std::map<std::string, CollectionNInfo> nInfo;
    vector<DDSSubmitParams> ddsParams;
    ddsParams = ddsSubmit.makeParams(plugin, resources, common, zoneInfo, nInfo, seconds(10));

    printParams(ddsParams);

    BOOST_TEST(ddsParams.size() == 3);
    testParameterSet(ddsParams.at(0), "slurm", "calib", "calib1", 1, 0, 1, 2, "/home/user/slurm-calib.cfg", "");
    testParameterSet(ddsParams.at(1), "slurm", "online", "online", 1, 0, 1, 0, "/home/user/slurm-online.cfg", "");
    testParameterSet(ddsParams.at(2), "slurm", "calib", "calib2", 1, 0, 1, 1, "/home/user/slurm-calib.cfg", "");
}

BOOST_AUTO_TEST_SUITE_END()

int main(int argc, char* argv[]) { return boost::unit_test::unit_test_main(init_unit_test, argc, argv); }
