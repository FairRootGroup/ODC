/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__odc_core_lib_fixtures
#define __ODC__odc_core_lib_fixtures

#include <odc/Logger.h>
#include <odc/MiscUtils.h>
#include <odc/Semaphore.h>
#include <odc/Session.h>
#include <odc/TopologyDefs.h>

#include <dds/Tools.h>
#include <dds/Topology.h>

#include <boost/asio/io_context.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test_log.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

struct AsyncOpFixture
{
    boost::asio::io_context mIoContext;
};

struct TopologyFixture
{
    TopologyFixture(std::string topoXMLPath)
        : mDDSTopo(std::move(topoXMLPath))
    {
        using namespace dds::tools_api;
        using namespace odc::core;

        mSession.mDDSSession.create();

        Logger::Config logConfig;
        logConfig.mSeverity = ESeverity::debug;
        std::stringstream ss;
        ss << mSession.mDDSSession.getSessionID();
        const boost::filesystem::path p{ boost::filesystem::temp_directory_path() / ss.str() };
        logConfig.mLogDir = p.string();

        try {
            Logger::instance().init(logConfig);
        } catch (std::exception& e) {
            std::cerr << "Can't initialize log: " << e.what() << std::endl;
        }

        SSubmitRequestData submitInfo;
        submitInfo.m_rms = "localhost";
        submitInfo.m_instances = 1;
        submitInfo.m_slots = mSlots;
        submitInfo.m_config = "";

        SharedSemaphore blocker;
        auto submitRequest = SSubmitRequest::makeRequest(submitInfo);
        submitRequest->setMessageCallback([](const SMessageResponseData& message) { BOOST_TEST_MESSAGE(message.m_msg); });
        submitRequest->setDoneCallback([blocker]() mutable { blocker.Signal(); });
        mSession.mDDSSession.sendRequest<SSubmitRequest>(submitRequest);
        blocker.Wait();

        std::size_t idleSlotsCount(0);
        int interval(8);
        while (idleSlotsCount < mSlots) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            interval = std::min(256, interval * 2);
            SAgentCountRequest::response_t res;
            mSession.mDDSSession.syncSendRequest<SAgentCountRequest>(SAgentCountRequest::request_t(), res);
            idleSlotsCount = res.m_idleSlotsCount;
        }

        STopologyRequestData topologyInfo;
        topologyInfo.m_updateType = STopologyRequestData::EUpdateType::ACTIVATE;
        topologyInfo.m_topologyFile = mDDSTopo.getFilepath();

        auto topologyRequest = STopologyRequest::makeRequest(topologyInfo);
        topologyRequest->setMessageCallback([](const SMessageResponseData& message) { BOOST_TEST_MESSAGE(message.m_msg); });

        std::mutex mtx;
        // fill the task/collection details
        topologyRequest->setResponseCallback([this, &mtx](const dds::tools_api::STopologyResponseData& res) {
            std::cout << "DDS Activate Response: "
                << "agentID: " << res.m_agentID
                << "; slotID: " << res.m_slotID
                << "; taskID: " << res.m_taskID
                << "; collectionID: " << res.m_collectionID
                << "; host: " << res.m_host
                << "; path: " << res.m_path
                << "; workDir: " << res.m_wrkDir
                << "; activated: " << res.m_activated
                << std::endl;

            // We are not interested in stopped tasks
            if (res.m_activated) {
                // response callbacks can be called in parallel - protect session access with a lock
                std::lock_guard<std::mutex> lock(mtx);
                mSession.mTaskDetails.emplace(res.m_taskID, TaskDetails{res.m_agentID, res.m_slotID, res.m_taskID, res.m_collectionID, res.m_path, res.m_host, res.m_wrkDir});

                if (res.m_collectionID > 0) {
                    if (mSession.mCollectionDetails.find(res.m_collectionID) == mSession.mCollectionDetails.end()) {
                        std::string path = res.m_path;
                        auto pos = path.rfind('/');
                        if (pos != std::string::npos) {
                            path.erase(pos);
                        }
                        mSession.mCollectionDetails.emplace(res.m_collectionID, CollectionDetails{res.m_agentID, res.m_collectionID, path, res.m_host, res.m_wrkDir});
                        mSession.mRuntimeCollectionIndex.at(res.m_collectionID)->mRuntimeCollectionAgents[res.m_collectionID] = res.m_agentID;
                    }
                }
            }
        });

        topologyRequest->setDoneCallback([blocker]() mutable { blocker.Signal(); });
        mSession.mDDSSession.sendRequest<STopologyRequest>(topologyRequest);
        blocker.Wait();

        std::size_t execSlotsCount(0);
        interval = 8;
        while (execSlotsCount < mSlots) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            interval = std::min(256, interval * 2);
            SAgentCountRequest::response_t res;
            mSession.mDDSSession.syncSendRequest<SAgentCountRequest>(SAgentCountRequest::request_t(), res);
            execSlotsCount = res.m_executingSlotsCount;
        }
    }

    ~TopologyFixture()
    {
        if (mSession.mDDSSession.IsRunning()) {
            mSession.mDDSSession.shutdown();
        }
    }

    static constexpr int mSlots = 6;
    odc::core::Session mSession;
    dds::topology_api::CTopology mDDSTopo;
    boost::asio::io_context mIoContext;
};

#endif
