/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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
#include <odc/MiscUtils.h>
#include <odc/Session.h>

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace boost::unit_test;

using namespace odc::core;
using namespace fair::mq;

using std::string;

void printSessionDetails(const Session& session)
{
    std::cout << "##### Session for partition: " << session.mPartitionID << std::endl;
    std::cout << "Topology file: " << session.mTopoFilePath << std::endl;

    std::cout << session.mZoneInfo.size() << " Zone(s):" << std::endl;
    for (const auto& z : session.mZoneInfo) {
        std::cout << "  " << quoted(z.first) << ":" << std::endl;
        for (const auto& zi : z.second) {
            std::cout << "    n: " << zi.n << "; nCores: " << zi.nCores << "; agentGroup: " << zi.agentGroup << std::endl;
        }
    }

    std::cout << session.mNinfo.size() << " N info(s):" << std::endl;
    for (const auto& [collection, nmin] : session.mNinfo) {
        std::cout << "  name: " << collection << "; "<< nmin << std::endl;
    }

    std::cout << session.mCollections.size() << " Collection(s):" << std::endl;
    for (const auto& col : session.mCollections) {
        std::cout << "  " << col.second << std::endl;
    }

    std::cout << session.mStandaloneTasks.size() << " Task(s) (outside of collections):" << std::endl;
    for (const auto& task : session.mStandaloneTasks) {
        std::cout << "  " << task << std::endl;
    }

    std::cout << session.mAgentGroupInfo.size() << " Agent group(s):" << std::endl;
    for (const auto& [groupName, agi] : session.mAgentGroupInfo) {
        std::cout << "  " << agi << std::endl;
    }
    std::cout << "###########" << std::endl;
}

void testZoneGroup(const ZoneGroup& zg, int32_t n, int32_t nCores, const std::string& agentGroup)
{
    BOOST_TEST(zg.n == n);
    BOOST_TEST(zg.nCores == nCores);
    BOOST_TEST(zg.agentGroup == agentGroup);
}

void testNinfo(const CollectionNInfo& cni, int32_t nOriginal, int32_t nMin, const std::string& agentGroup)
{
    BOOST_TEST(cni.nOriginal == nOriginal);
    BOOST_TEST(cni.nMin == nMin);
    BOOST_TEST(cni.agentGroup == agentGroup);
}

void testCollection(
    const CollectionInfo& colInfo,
    const string& name,
    const string& zone,
    const string& agentGroup,
    int32_t nOriginal,
    size_t nMin,
    size_t nCores,
    size_t numTasks,
    size_t totalTasks)
{
    BOOST_TEST(colInfo.name == name);
    BOOST_TEST(colInfo.zone == zone);
    BOOST_TEST(colInfo.agentGroup == agentGroup);
    BOOST_TEST(colInfo.nOriginal == nOriginal);
    BOOST_TEST(colInfo.nMin == nMin);
    BOOST_TEST(colInfo.nCores == nCores);
    BOOST_TEST(colInfo.numTasks == numTasks);
    BOOST_TEST(colInfo.totalTasks == totalTasks);
}

void testAgentGroupInfo(
    const AgentGroupInfo& agi,
    const string& name,
    const string& zone,
    int32_t numAgents,
    size_t minAgents,
    size_t numSlots,
    size_t numCores)
{
    BOOST_TEST(agi.name == name);
    BOOST_TEST(agi.zone == zone);
    BOOST_TEST(agi.numAgents == numAgents);
    BOOST_TEST(agi.minAgents == minAgents);
    BOOST_TEST(agi.numSlots == numSlots);
    BOOST_TEST(agi.numCores == numCores);
}

BOOST_AUTO_TEST_SUITE(extraction)

BOOST_AUTO_TEST_CASE(simple)
{
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    Session session;
    session.mPartitionID = partitionId;
    session.mTopoFilePath = kODCDataDir + "/ex-topo-infinite.xml";
    Controller::extractRequirements(common, session);

    printSessionDetails(session);

    BOOST_TEST(session.mZoneInfo.size() == 0);
    BOOST_TEST(session.mNinfo.size() == 0);
    BOOST_TEST(session.mCollections.size() == 1);
    testCollection(session.mCollections.at("EPNCollection"), "EPNCollection", "", "", 1, -1, 0, 12, 12);

    BOOST_TEST(session.mAgentGroupInfo.size() == 1);
    testAgentGroupInfo(session.mAgentGroupInfo.at(""), "", "", 1, -1, 12, 0);
}

BOOST_AUTO_TEST_CASE(zones_from_agent_groupnames)
{
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    Session session;
    session.mPartitionID = partitionId;
    // Here the zones are not explicitly defined, and derived from the agent groups
    session.mTopoFilePath = kODCDataDir + "/ex-topo-groupname.xml";
    Controller::extractRequirements(common, session);

    printSessionDetails(session);

    BOOST_TEST(session.mZoneInfo.size() == 2);
    BOOST_TEST(session.mZoneInfo.at("calib").size() == 1);
    testZoneGroup(session.mZoneInfo.at("calib").at(0), 1, 0, "calib");
    BOOST_TEST(session.mZoneInfo.at("online").size() == 1);
    testZoneGroup(session.mZoneInfo.at("online").at(0), 4, 0, "online");

    BOOST_TEST(session.mCollections.size() == 2);
    testCollection(session.mCollections.at("SamplersSinks"), "SamplersSinks", "calib", "calib", 1, -1, 0, 2, 2);
    testCollection(session.mCollections.at("Processors"), "Processors", "online", "online", 4, 2, 0, 2, 8);

    BOOST_TEST(session.mAgentGroupInfo.size() == 2);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib"), "calib", "calib", 1, -1, 2, 0);
    testAgentGroupInfo(session.mAgentGroupInfo.at("online"), "online", "online", 4, 2, 2, 0);
}

BOOST_AUTO_TEST_CASE(zones_with_ncores)
{
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    Session session;
    session.mPartitionID = partitionId;
    // Here the zones are not explicitly defined, and derived from the agent groups
    session.mTopoFilePath = kODCDataDir + "/ex-topo-groupname-ncores.xml";
    Controller::extractRequirements(common, session);

    printSessionDetails(session);

    BOOST_TEST(session.mZoneInfo.size() == 2);
    BOOST_TEST(session.mZoneInfo.at("calib").size() == 2);
    testZoneGroup(session.mZoneInfo.at("calib").at(0), 1, 2, "calib1");
    testZoneGroup(session.mZoneInfo.at("calib").at(1), 1, 1, "calib2");
    BOOST_TEST(session.mZoneInfo.at("online").size() == 1);
    testZoneGroup(session.mZoneInfo.at("online").at(0), 4, 0, "online");

    BOOST_TEST(session.mCollections.size() == 3);
    testCollection(session.mCollections.at("Samplers"), "Samplers", "calib", "calib1", 1, -1, 2, 1, 1);
    testCollection(session.mCollections.at("Sinks"), "Sinks", "calib", "calib2", 1, -1, 1, 1, 1);
    testCollection(session.mCollections.at("Processors"), "Processors", "online", "online", 4, -1, 0, 1, 4);

    BOOST_TEST(session.mAgentGroupInfo.size() == 3);
    testAgentGroupInfo(session.mAgentGroupInfo.at("online"), "online", "online", 4, -1, 1, 0);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib1"), "calib1", "calib", 1, -1, 1, 2);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib2"), "calib2", "calib", 1, -1, 1, 1);
}

BOOST_AUTO_TEST_CASE(nmin)
{
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    Session session;
    session.mPartitionID = partitionId;
    // Here the zones are not explicitly defined, and derived from the agent groups
    session.mTopoFilePath = kODCDataDir + "/ex-topo-groupname-crashing.xml";
    Controller::extractRequirements(common, session);

    printSessionDetails(session);

    BOOST_TEST(session.mZoneInfo.size() == 2);
    BOOST_TEST(session.mZoneInfo.at("calib").size() == 1);
    testZoneGroup(session.mZoneInfo.at("calib").at(0), 1, 0, "calib");
    BOOST_TEST(session.mZoneInfo.at("online").size() == 1);
    testZoneGroup(session.mZoneInfo.at("online").at(0), 4, 0, "online");

    BOOST_TEST(session.mNinfo.size() == 2);
    testNinfo(session.mNinfo.at("SamplersSinks"), 1, -1, "calib");
    testNinfo(session.mNinfo.at("Processors"), 4, 2, "online");

    BOOST_TEST(session.mCollections.size() == 2);
    testCollection(session.mCollections.at("SamplersSinks"), "SamplersSinks", "calib", "calib", 1, -1, 0, 2, 2);
    testCollection(session.mCollections.at("Processors"), "Processors", "online", "online", 4, 2, 0, 2, 8);

    BOOST_TEST(session.mAgentGroupInfo.size() == 2);
    testAgentGroupInfo(session.mAgentGroupInfo.at("online"), "online", "online", 4, 2, 2, 0);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib"), "calib", "calib", 1, -1, 2, 0);
}

BOOST_AUTO_TEST_CASE(epn)
{
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    Session session;
    session.mPartitionID = partitionId;
    // Here the zones are not explicitly defined, and derived from the agent groups
    session.mTopoFilePath = kODCDataDir + "/ex-epn.xml";
    Controller::extractRequirements(common, session);

    printSessionDetails(session);

    BOOST_TEST(session.mZoneInfo.size() == 2);
    BOOST_TEST(session.mZoneInfo.at("calib").size() == 1);
    testZoneGroup(session.mZoneInfo.at("calib").at(0), 1, 128, "calib1");
    BOOST_TEST(session.mZoneInfo.at("online").size() == 1);
    testZoneGroup(session.mZoneInfo.at("online").at(0), 50, 0, "online");

    BOOST_TEST(session.mNinfo.size() == 2);
    testNinfo(session.mNinfo.at("wf11.dds"), 1, -1, "calib1");
    testNinfo(session.mNinfo.at("RecoCollection"), 50, 50, "online");

    BOOST_TEST(session.mCollections.size() == 2);
    testCollection(session.mCollections.at("wf11.dds"), "wf11.dds", "calib", "calib1", 1, -1, 128, 17, 17);
    testCollection(session.mCollections.at("RecoCollection"), "RecoCollection", "online", "online", 50, 50, 0, 223, 11150);

    BOOST_TEST(session.mAgentGroupInfo.size() == 2);
    testAgentGroupInfo(session.mAgentGroupInfo.at("online"), "online", "online", 50, 50, 223, 0);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib1"), "calib1", "calib", 1, -1, 17, 128);
}

BOOST_AUTO_TEST_CASE(epn_2)
{
    string partitionId = "test_partition_" + uuid();
    CommonParams common(partitionId, 0, 10);
    Session session;
    session.mPartitionID = partitionId;
    // Here the zones are not explicitly defined, and derived from the agent groups
    session.mTopoFilePath = kODCDataDir + "/ex-epn-2.xml";
    Controller::extractRequirements(common, session);

    printSessionDetails(session);

    BOOST_TEST(session.mZoneInfo.size() == 2);
    BOOST_TEST(session.mZoneInfo.at("online").size() == 1);
    testZoneGroup(session.mZoneInfo.at("online").at(0), 108, 0, "online");
    BOOST_TEST(session.mZoneInfo.at("calib").size() == 4);
    testZoneGroup(session.mZoneInfo.at("calib").at(0), 1, 32, "calib1");
    testZoneGroup(session.mZoneInfo.at("calib").at(1), 1, 32, "calib2");
    testZoneGroup(session.mZoneInfo.at("calib").at(2), 1, 16, "calib3");
    testZoneGroup(session.mZoneInfo.at("calib").at(3), 1, 16, "calib4");

    BOOST_TEST(session.mNinfo.size() == 5);
    testNinfo(session.mNinfo.at("RecoCollection"), 108, 93, "online");
    testNinfo(session.mNinfo.at("wf5.dds"), 1, -1, "calib1");
    testNinfo(session.mNinfo.at("wf4.dds"), 1, -1, "calib2");
    testNinfo(session.mNinfo.at("wf10.dds"), 1, -1, "calib3");
    testNinfo(session.mNinfo.at("wf6.dds"), 1, -1, "calib4");

    BOOST_TEST(session.mCollections.size() == 5);
    testCollection(session.mCollections.at("RecoCollection"), "RecoCollection", "online", "online", 108, 93, 0, 351, 37908);
    testCollection(session.mCollections.at("wf5.dds"), "wf5.dds", "calib", "calib1", 1, -1, 32, 7, 7);
    testCollection(session.mCollections.at("wf4.dds"), "wf4.dds", "calib", "calib2", 1, -1, 32, 13, 13);
    testCollection(session.mCollections.at("wf10.dds"), "wf10.dds", "calib", "calib3", 1, -1, 16, 7, 7);
    testCollection(session.mCollections.at("wf6.dds"), "wf6.dds", "calib", "calib4", 1, -1, 16, 13, 13);

    BOOST_TEST(session.mAgentGroupInfo.size() == 5);
    testAgentGroupInfo(session.mAgentGroupInfo.at("online"), "online", "online", 108, 93, 351, 0);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib1"), "calib1", "calib", 1, -1, 7, 32);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib2"), "calib2", "calib", 1, -1, 13, 32);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib3"), "calib3", "calib", 1, -1, 7, 16);
    testAgentGroupInfo(session.mAgentGroupInfo.at("calib4"), "calib4", "calib", 1, -1, 13, 16);
}

BOOST_AUTO_TEST_SUITE_END()

int main(int argc, char* argv[]) { return boost::unit_test::unit_test_main(init_unit_test, argc, argv); }
