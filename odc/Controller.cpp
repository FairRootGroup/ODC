/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <odc/Controller.h>
#include <odc/DDSSubmit.h>
#include <odc/Error.h>
#include <odc/Logger.h>
#include <odc/Process.h>
#include <odc/Restore.h>
#include <odc/Topology.h>

#include <dds/TopoCreator.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <algorithm>
#include <cctype> // std::tolower
#include <filesystem>
#include <iterator> // std::distance

using namespace odc;
using namespace odc::core;
using namespace std;
namespace bfs = boost::filesystem;

RequestResult Controller::execInitialize(const CommonParams& common, const InitializeParams& params)
{
    Error error;
    try {
        auto& partition = acquirePartition(common);

        if (params.mDDSSessionID.empty()) {
            // Create new DDS session
            // Shutdown DDS session if it is running already
            shutdownDDSSession(common, partition, error)
                && createDDSSession(common, *(partition.mSession), error);
        } else {
            // Attach to an existing DDS session
            bool success = attachToDDSSession(common, *(partition.mSession), error, params.mDDSSessionID);
            if (success) {
                // Request current active topology, if any
                partition.mSession->mTopoFilePath = getActiveDDSTopology(common, *(partition.mSession), error);
                // If a topology is active, create DDS and FairMQ topology objects
                if (!partition.mSession->mTopoFilePath.empty()) {
                    createDDSTopology(common, *(partition.mSession), error)
                        && createTopology(common, partition, error);
                }
            }
        }
        updateRestore();
        return createRequestResult(common, *(partition.mSession), error, "Initialize done", TopologyState(), "", {});
    } catch (exception& e) {
        fillAndLogFatalError(common, error, ErrorCode::RuntimeError, e.what());
        return createRequestResult(common, "", error, "Initialize failed", TopologyState(), "", {});
    }
}

RequestResult Controller::execSubmit(const CommonParams& common, const SubmitParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    auto [hosts, rmsJobIDs] = submit(common, *(partition.mSession), error, params.mPlugin, params.mResources, false);

    return createRequestResult(common, *(partition.mSession), error, "Submit done", TopologyState(), rmsJobIDs, hosts);
}

pair<unordered_set<string>, string> Controller::submit(const CommonParams& common, Session& session, Error& error, const string& plugin, const string& res, bool extractResources)
{
    unordered_set<string> hosts;
    string rmsJobIDs;
    if (extractResources) {
        OLOG(info, common) << "Attempting submission with resources extracted from topology.";
    } else {
        OLOG(info, common) << "Attempting submission with resources: " << res;
    }

    if (!session.mDDSSession.IsRunning()) {
        fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, "DDS session is not running. Use Init or Run to start the session.");
        return std::make_pair(hosts, rmsJobIDs);
    }

    // Get DDS submit parameters from ODC resource plugin
    vector<DDSSubmitParams> ddsParams;
    if (!error.mCode) {
        try {
            if (extractResources) {
                ddsParams = mSubmit.makeParams(mRMS, mZoneCfgs, session.mAgentGroupInfo);
            } else {
                ddsParams = mSubmit.makeParams(plugin, res, common, session.mZoneInfo, session.mNinfo, requestTimeout(common, "submit::MakeParams"));
            }
        } catch (Error& e) {
            error = e;
            OLOG(error, common) << "Resource plugin failed: " << e;
            return std::make_pair(hosts, rmsJobIDs);
        } catch (exception& e) {
            fillAndLogError(common, error, ErrorCode::ResourcePluginFailed, toString("Resource plugin failed: ", e.what()));
            return std::make_pair(hosts, rmsJobIDs);
        }
    }

    // initial expected number of slots from the current total slots, which may have been updated by previous submissions
    size_t expectedNumSlots = session.mTotalSlots;
    bool topologyChanged = false;

    if (!error.mCode) {
        OLOG(info, common) << "Preparing to submit " << ddsParams.size() << " configurations:";
        for (unsigned int i = 0; i < ddsParams.size(); ++i) {
            OLOG(info, common) << "  [" << i + 1 << "/" << ddsParams.size() << "]: " << ddsParams.at(i);
        }

        for (unsigned int i = 0; i < ddsParams.size(); ++i) {
            OLOG(info, common) << "Submitting [" << i + 1 << "/" << ddsParams.size() << "]: " << ddsParams.at(i);
            std::string groupName = ddsParams.at(i).mAgentGroup;
            int32_t minCount = ddsParams.at(i).mMinAgents;
            uint32_t numExpectedAgents = ddsParams.at(i).mNumAgents;
            uint32_t numSubmittedAgents = submitDDSAgents(common, session, error, ddsParams.at(i));
            if (numSubmittedAgents == numExpectedAgents) {
                expectedNumSlots += numSubmittedAgents * ddsParams.at(i).mNumSlots;
            } else {
                OLOG(info, common) << "Submitted " << numSubmittedAgents << " agents instead of " << numExpectedAgents;
                if (minCount == 0) { // nMin is not defined
                    fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Number of agents (", numSubmittedAgents, ") for group ", groupName, " is less than requested (", numExpectedAgents, "), " , "and no nMin is defined"));
                    break;
                }
                if (numSubmittedAgents < minCount) { // count is less than nMin
                    fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Number of agents (", numSubmittedAgents, ") for group ", groupName, " is less than requested (", numExpectedAgents, "), " , "and nMin (", minCount, ") is not satisfied"));
                    break;
                }
                OLOG(info, common) << "Number of agents (" << numSubmittedAgents << ") for group " << groupName << " is less than requested (" << numExpectedAgents << "), " << "but nMin (" << minCount << ") is satisfied";
                expectedNumSlots += numSubmittedAgents * ddsParams.at(i).mNumSlots;

                // update nCurrent for the agent group
                auto ni =  std::find_if(session.mNinfo.begin(), session.mNinfo.end(), [&](const auto& _ni) {
                    return _ni.second.agentGroup == groupName;
                });
                if (ni != session.mNinfo.end()) {
                    ni->second.nCurrent = numSubmittedAgents;
                } else {
                    OLOG(error, common) << "Agent group info not found for group " << groupName;
                }

                // update topology
                topologyChanged = true;
            }
        }
        if (!error.mCode) {
            if (topologyChanged) {
                updateTopology(common, session);
            }

            OLOG(info, common) << "Waiting for " << expectedNumSlots << " slots...";
            // TODO: wait not for total number of slots, but count each group individually
            if (waitForNumActiveSlots(common, session, error, expectedNumSlots)) {
                session.mTotalSlots = expectedNumSlots;
                OLOG(info, common) << "Done waiting for " << expectedNumSlots << " slots.";
            }
        }
    }

    if (session.mDDSSession.IsRunning()) {
        map<string, uint32_t> agentCounts; // agent count sorted by their group name

        for (const auto& p : ddsParams) {
            agentCounts[p.mAgentGroup] = 0;
        }

        try {
            auto agentInfo = getAgentInfo(common, session);
            OLOG(info, common) << "Launched " << agentInfo.size() << " DDS agents:";
            hosts.reserve(agentInfo.size());
            for (const auto& ai : agentInfo) {
                agentCounts[ai.m_groupName]++;
                session.mAgentInfo[ai.m_agentID].numSlots = ai.m_nSlots;

                std::string rmsJobID;
                auto agiIt = std::find_if(session.mAgentGroupInfo.begin(), session.mAgentGroupInfo.end(), [&ai](const AgentGroupInfo& info) {
                    return info.name == ai.m_groupName;
                });
                if (agiIt != session.mAgentGroupInfo.end()) {
                    session.mAgentInfo.at(ai.m_agentID).agentGroupInfoIndex = std::distance(session.mAgentGroupInfo.begin(), agiIt);
                    rmsJobID = agiIt->rmsJobID;
                } else {
                    OLOG(error, common) << "Agent group info not found for agent " << ai.m_agentID;
                }

                hosts.emplace(ai.m_host);
                rmsJobIDs += rmsJobID + " ";
                OLOG(info, common)
                    << "  Agent ID: " << ai.m_agentID
                    // << ", pid: " << ai.m_agentPid
                    << "; host: " << ai.m_host
                    << "; path: " << ai.m_DDSPath
                    << "; group: " << ai.m_groupName
                    << "; rmsJobID: " << std::quoted(rmsJobID)
                    // << "; index: " << ai.m_index
                    // << "; username: " << ai.m_username
                    << "; startup time: " << ai.m_startUpTime.count() << " ms"
                    << "; slots: " << ai.m_nSlots;
                    // << " (idle: " << ai.m_nIdleSlots
                    // << ", executing: " << ai.m_nExecutingSlots << ").";
            }
            OLOG(info, common) << "Launched " << agentCounts.size() << " DDS agent groups:";
            for (const auto& [groupName, count] : agentCounts) {
                OLOG(info, common) << "  " << std::quoted(groupName) << ": " << count << " agents";
            }
        } catch (Error& e) {
            error = e;
            OLOG(error, common) << "Failed getting agent info: " << e;
        } catch (const exception& e) {
            fillAndLogError(common, error, ErrorCode::DDSCommanderInfoFailed, toString("Failed getting agent info: ", e.what()));
        }

        if (error.mCode) {
            attemptSubmitRecovery(common, session, error, ddsParams, agentCounts);
        }
    }

    return std::make_pair(hosts, rmsJobIDs);
}

uint32_t Controller::submitDDSAgents(const CommonParams& common, Session& session, Error& error, const DDSSubmitParams& params)
{
    using namespace dds::tools_api;

    uint32_t numSubmittedAgents = params.mNumAgents;

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_submissionTag = common.mPartitionID;
    requestInfo.m_rms = params.mRMS;
    requestInfo.m_instances = params.mNumAgents;
    requestInfo.m_minInstances = params.mMinAgents;
    // requestInfo.m_minInstances = 0;
    // OLOG(debug, common) << "Ignoring params.mMinAgents for DDS submission to avoid nMin handling temporarily";
    requestInfo.m_slots = params.mNumSlots;
    requestInfo.m_config = params.mConfigFile;
    requestInfo.m_envCfgFilePath = params.mEnvFile;
    requestInfo.m_groupName = params.mAgentGroup;

    // DDS does not support ncores parameter directly, set it here through additional config in case of Slurm
    if (params.mRMS == "slurm" && params.mNumCores > 0) {
        // the following disables `#SBATCH --cpus-per-task=%DDS_NSLOTS%` of DDS for Slurm
        requestInfo.setFlag(SSubmitRequestData::ESubmitRequestFlags::enable_overbooking, true);

        requestInfo.m_inlineConfig = string("#SBATCH --cpus-per-task=" + to_string(params.mNumCores));
    }

    OLOG(info, common) << "Submitting: " << requestInfo;

    bool done = false;
    mutex mtx;
    condition_variable cv;

    SSubmitRequest::ptr_t requestPtr = SSubmitRequest::makeRequest(requestInfo);

    requestPtr->setMessageCallback([&numSubmittedAgents, &error, &common, this](const SMessageResponseData& msg) {
        if (msg.m_severity == dds::intercom_api::EMsgSeverity::error) {
            numSubmittedAgents = 0;
            fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Submit error: ", msg.m_msg));
        } else {
            OLOG(info, common) << "...Submit: " << msg.m_msg;
        }
    });

    requestPtr->setResponseCallback([&numSubmittedAgents, &common, &session, &params](const SSubmitResponseData& res) {
        OLOG(info, common) << "Submission details:";

        if (!res.m_jobIDs.empty()) {
            auto agiIt = std::find_if(session.mAgentGroupInfo.begin(), session.mAgentGroupInfo.end(), [&params](const AgentGroupInfo& agi) {
                return agi.name == params.mAgentGroup;
            });
            if (agiIt == session.mAgentGroupInfo.end()) {
                OLOG(warning, common) << "Agent group info not found for " << params.mAgentGroup;
            } else {
                agiIt->rmsJobID = strVecToStr(res.m_jobIDs);
            }
        }

        if (res.m_jobInfoAvailable) {
            OLOG(info, common) << "Allocated " << res.m_allocNodes << " nodes for " << params.mAgentGroup << " agent group";
            // when not using core-based scheduling, number of allocated nodes equals number of agents
            numSubmittedAgents = res.m_allocNodes;
        } else {
            OLOG(debug, common) << "Warning: Job information is not fully available";
        }
    });

    requestPtr->setDoneCallback([&mtx, &cv, &done]() {
        {
            lock_guard<mutex> lock(mtx);
            done = true;
        }
        cv.notify_all();
    });

    session.mDDSSession.sendRequest<SSubmitRequest>(requestPtr);

    try {
        unique_lock<mutex> lock(mtx);
        cv.wait_for(lock, requestTimeout(common, "wait_for lock in submitDDSAgents"), [&done]{ return done; });

        if (!done) {
            numSubmittedAgents = 0;
            fillAndLogError(common, error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission for agent group " + params.mAgentGroup);

            requestPtr->unsubscribeAll();
        } else {
            // OLOG(info, common) << "Agent submission done successfully";
        }
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Agent submission error: " << e;
        numSubmittedAgents = 0;
    }

    return numSubmittedAgents;
}

bool Controller::waitForNumActiveSlots(const CommonParams& common, Session& session, Error& error, size_t numSlots)
{
    try {
        if (!mAgentWaitTimeout.empty()) {
            std::chrono::seconds configuredTimeoutS = (common.mTimeout == 0 ? mTimeout : std::chrono::seconds(common.mTimeout));
            std::chrono::seconds duration = parseTimeString(mAgentWaitTimeout, configuredTimeoutS);
            OLOG(debug, common) << "waitForNumActiveSlots: Using --agent-wait-timeout timeout ('" << mAgentWaitTimeout << "') --> " << duration.count() << " seconds";
            session.mDDSSession.waitForNumSlots<dds::tools_api::CSession::EAgentState::active>(numSlots, duration);
        } else {
            OLOG(debug, common) << "waitForNumActiveSlots: Using default timeout";
            session.mDDSSession.waitForNumSlots<dds::tools_api::CSession::EAgentState::active>(numSlots, requestTimeout(common, "waitForNumActiveSlots..waitForNumSlots<dds::tools_api::CSession::EAgentState::active>"));
        }
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Error while waiting for DDS slots: " << e;
        return false;
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::RequestTimeout, toString("Timeout waiting for DDS slots: ", e.what()));
        return false;
    }
    return true;
}

void Controller::attemptSubmitRecovery(const CommonParams& common, Session& session, Error& error, const vector<DDSSubmitParams>& ddsParams, const map<string, uint32_t>& agentCounts)
{
    error = Error();
    try {
        for (const auto& p : ddsParams) {
            uint32_t actualCount = agentCounts.at(p.mAgentGroup);
            uint32_t requestedCount = p.mNumAgents;
            uint32_t minCount = p.mMinAgents;

            if (requestedCount != actualCount) {
                // fail recovery if insufficient agents, and no nMin is defined
                if (minCount == 0) {
                    fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Number of agents (", actualCount, ") for group ", p.mAgentGroup, " is less than requested (", requestedCount, "), " , "and no nMin is defined"));
                    return;
                }
                // fail recovery if insufficient agents, and actual count is less than nMin
                if (actualCount < minCount) {
                    fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Number of agents (", actualCount, ") for group ", p.mAgentGroup, " is less than requested (", requestedCount, "), " , "and nMin (", minCount, ") is not satisfied"));
                    return;
                }
                OLOG(info, common) << "Number of agents (" << actualCount << ") for group " << p.mAgentGroup << " is less than requested (" << requestedCount << "), " << "but nMin (" << minCount << ") is satisfied";
            }
        }
        if (!error.mCode) {
            try {
                session.mTotalSlots = getNumSlots(common, session);
                for (auto& ni : session.mNinfo) {
                    auto it = find_if(agentCounts.cbegin(), agentCounts.cend(), [&](const auto& ac) {
                        return ac.first == ni.second.agentGroup;
                    });
                    if (it != agentCounts.cend()) {
                        ni.second.nCurrent = it->second;
                    }
                }
                updateTopology(common, session);
            } catch (const exception& e) {
                fillAndLogError(common, error, ErrorCode::DDSCreateTopologyFailed, toString("Failed updating topology: ", e.what()));
            }
        }
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Submit recovery failed: " << e;
    } catch (const exception& e) {
        fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Submit recovery failed: ", e.what()));
    }
}

void Controller::updateTopology(const CommonParams& common, Session& session)
{
    using namespace dds::topology_api;
    OLOG(info, common) << "Updating current topology file (" << session.mTopoFilePath << ") to reflect the reduced number of groups";

    CTopoCreator creator;
    creator.getMainGroup()->initFromXML(session.mTopoFilePath);

    auto collections = creator.getMainGroup()->getElementsByType(CTopoBase::EType::COLLECTION);
    for (auto& col : collections) {
        // if a collection has nMin setting, update the n value of the group it is in to the current n
        auto it = session.mNinfo.find(col->getName());
        if (it != session.mNinfo.end()) {
            auto parent = col->getParent();
            if (parent->getType() == CTopoBase::EType::GROUP && parent->getName() != "main") {
                OLOG(info, common) << "nMin: updating `n` for collection " << std::quoted(col->getName()) << " to " << it->second.nCurrent;
                static_cast<CTopoGroup*>(parent)->setN(it->second.nCurrent);
            } else {
                OLOG(error, common) << "nMin: collection " << std::quoted(col->getName()) << " is not in a group. Not updating it's `n` value.";
            }
        } else {
            OLOG(info, common) << "nMin: collection " << std::quoted(col->getName()) << " has no nMin setting. Nothing to update";
        }
    }

    string name("topo_" + session.mPartitionID + "_reduced.xml");
    const bfs::path tmpPath{ bfs::temp_directory_path() / bfs::unique_path() };
    bfs::create_directories(tmpPath);
    const bfs::path filepath{ tmpPath / name };
    creator.save(filepath.string());

    session.mTopoFilePath = filepath.string();

    OLOG(info, common) << "Saved updated topology file as " << session.mTopoFilePath;
}

RequestResult Controller::execActivate(const CommonParams& common, const ActivateParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    if (!partition.mSession->mDDSSession.IsRunning()) {
        fillAndLogError(common, error, ErrorCode::DDSActivateTopologyFailed, "DDS session is not running. Use Init or Run to start the session.");
    }

    try {
        partition.mSession->mTopoFilePath = topoFilepath(common, params.mTopoFile, params.mTopoContent, params.mTopoScript);
        extractRequirements(common, *(partition.mSession));
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Activate failed: " << e;
    } catch (exception& e) {
        fillAndLogFatalError(common, error, ErrorCode::TopologyFailed, e.what());
    }

    if (!error.mCode) {
        activate(common, partition, error);
    }

    TopologyState topologyState(error.mCode ? AggregatedState::Undefined : AggregatedState::Idle);
    return createRequestResult(common, *(partition.mSession), error, "Activate done", std::move(topologyState), "", {});
}

void Controller::activate(const CommonParams& common, Partition& partition, Error& error)
{
    activateDDSTopology(common, *(partition.mSession), error, dds::tools_api::STopologyRequest::request_t::EUpdateType::ACTIVATE)
        && createDDSTopology(common, *(partition.mSession), error)
        && createTopology(common, partition, error)
        && waitForState(common, partition, error, "", DeviceState::Idle);
}

RequestResult Controller::execRun(const CommonParams& common, const RunParams& params)
{
    Error error;
    try {
        auto& partition = acquirePartition(common);
        std::unordered_set<std::string> hosts;
        std::string rmsJobIDs;

        if (!partition.mSession->mRunAttempted) {
            partition.mSession->mRunAttempted = true;

            // Create new DDS session
            // Shutdown DDS session if it is running already
            shutdownDDSSession(common, partition, error)
                && createDDSSession(common, *(partition.mSession), error);

            updateRestore();

            if (!error.mCode) {
                try {
                    partition.mSession->mTopoFilePath = topoFilepath(common, params.mTopoFile, params.mTopoContent, params.mTopoScript);
                    extractRequirements(common, *(partition.mSession));
                } catch (Error& e) {
                    error = e;
                    OLOG(error, common) << "Topology creation failed: " << e;
                } catch (exception& e) {
                    fillAndLogFatalError(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
                }

                if (!error.mCode) {
                    if (!partition.mSession->mDDSSession.IsRunning()) {
                        fillAndLogError(common, error, ErrorCode::DDSSubmitAgentsFailed, "DDS session is not running. Use Init or Run to start the session.");
                    }

                    std::tie(hosts, rmsJobIDs) = submit(common, *(partition.mSession), error, params.mPlugin, params.mResources, params.mExtractTopoResources);

                    if (!partition.mSession->mDDSSession.IsRunning()) {
                        fillAndLogError(common, error, ErrorCode::DDSActivateTopologyFailed, "DDS session is not running. Use Init or Run to start the session.");
                    }

                    if (!error.mCode) {
                        activate(common, partition, error);
                    }
                }
            }
        } else {
            error = Error(MakeErrorCode(ErrorCode::RequestNotSupported), "Repeated Run request is not supported. Shutdown this partition to retry.");
        }

        TopologyState topologyState(error.mCode ? AggregatedState::Undefined : AggregatedState::Idle);
        return createRequestResult(common, *(partition.mSession), error, "Run done", std::move(topologyState), rmsJobIDs, hosts);
    } catch (exception& e) {
        fillAndLogFatalError(common, error, ErrorCode::RuntimeError, e.what());
        return createRequestResult(common, "", error, "Run failed", TopologyState(), "", {});
    }
}

RequestResult Controller::execUpdate(const CommonParams& common, const UpdateParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    TopologyState topologyState;

    try {
        partition.mSession->mTopoFilePath = topoFilepath(common, params.mTopoFile, params.mTopoContent, params.mTopoScript);
        extractRequirements(common, *(partition.mSession));
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Topology creation failed: " << e;
    } catch (exception& e) {
        fillAndLogFatalError(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
    }

    if (!error.mCode) {
        changeStateReset(common, partition, error, "", topologyState)
            && resetTopology(partition)
            && activateDDSTopology(common, *(partition.mSession), error, dds::tools_api::STopologyRequest::request_t::EUpdateType::UPDATE)
            && createDDSTopology(common, *(partition.mSession), error)
            && createTopology(common, partition, error)
            && waitForState(common, partition, error, "", DeviceState::Idle)
            && changeStateConfigure(common, partition, error, "", topologyState);
    }
    return createRequestResult(common, *(partition.mSession), error, "Update done", std::move(topologyState), "", {});
}

RequestResult Controller::execShutdown(const CommonParams& common)
{
    Error error;

    // grab the session id before shutting down the session, to return it in the reply
    string ddsSessionId;
    {
        auto& partition = acquirePartition(common);
        ddsSessionId = to_string(partition.mSession->mDDSSession.getSessionID());
        shutdownDDSSession(common, partition, error);
    }

    removePartition(common);
    updateRestore();

    return createRequestResult(common, ddsSessionId, error, "Shutdown done", TopologyState(), "", {});
}

RequestResult Controller::execSetProperties(const CommonParams& common, const SetPropertiesParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    TopologyState topologyState;
    setProperties(common, partition, error, params.mPath, params.mProperties, topologyState);
    return createRequestResult(common, *(partition.mSession), error, "SetProperties done", std::move(topologyState), "", {});
}

RequestResult Controller::execGetState(const CommonParams& common, const DeviceParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    TopologyState topologyState(AggregatedState::Undefined, params.mDetailed ? std::make_optional<DetailedState>() : std::nullopt);
    getState(common, partition, error, params.mPath, topologyState);
    return createRequestResult(common, *(partition.mSession), error, "GetState done", std::move(topologyState), "", {});
}

RequestResult Controller::execConfigure(const CommonParams& common, const DeviceParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    TopologyState topologyState(AggregatedState::Undefined, params.mDetailed ? std::make_optional<DetailedState>() : std::nullopt);
    changeStateConfigure(common, partition, error, params.mPath, topologyState);
    return createRequestResult(common, *(partition.mSession), error, "Configure done", std::move(topologyState), "", {});
}

RequestResult Controller::execStart(const CommonParams& common, const DeviceParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    // update run number
    partition.mSession->mLastRunNr.store(common.mRunNr);

    TopologyState topologyState(AggregatedState::Undefined, params.mDetailed ? std::make_optional<DetailedState>() : std::nullopt);
    changeState(common, partition, error, params.mPath, TopoTransition::Run, topologyState);
    return createRequestResult(common, *(partition.mSession), error, "Start done", std::move(topologyState), "", {});
}

RequestResult Controller::execStop(const CommonParams& common, const DeviceParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    TopologyState topologyState(AggregatedState::Undefined, params.mDetailed ? std::make_optional<DetailedState>() : std::nullopt);
    changeState(common, partition, error, params.mPath, TopoTransition::Stop, topologyState);

    // reset the run number, which is valid only for the running state
    partition.mSession->mLastRunNr.store(0);

    return createRequestResult(common, *(partition.mSession), error, "Stop done", std::move(topologyState), "", {});
}

RequestResult Controller::execReset(const CommonParams& common, const DeviceParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    TopologyState topologyState(AggregatedState::Undefined, params.mDetailed ? std::make_optional<DetailedState>() : std::nullopt);
    changeStateReset(common, partition, error, params.mPath, topologyState);
    return createRequestResult(common, *(partition.mSession), error, "Reset done", std::move(topologyState), "", {});
}

RequestResult Controller::execTerminate(const CommonParams& common, const DeviceParams& params)
{
    Error error;
    auto& partition = acquirePartition(common);

    TopologyState topologyState(AggregatedState::Undefined, params.mDetailed ? std::make_optional<DetailedState>() : std::nullopt);
    changeState(common, partition, error, params.mPath, TopoTransition::End, topologyState);
    return createRequestResult(common, *(partition.mSession), error, "Terminate done", std::move(topologyState), "", {});
}

StatusRequestResult Controller::execStatus(const StatusParams& params)
{
    lock_guard<mutex> lock(mPartitionMtx);

    StatusRequestResult result;
    for (const auto& p : mPartitions) {
        const auto& session = p.second.mSession;
        const auto& topology = p.second.mTopology;
        PartitionStatus status;
        status.mPartitionID = session->mPartitionID;
        try {
            status.mDDSSessionID = to_string(session->mDDSSession.getSessionID());
            status.mDDSSessionStatus = (session->mDDSSession.IsRunning()) ? DDSSessionStatus::running : DDSSessionStatus::stopped;
        } catch (exception& e) {
            OLOG(warning, status.mPartitionID, 0) << "Failed to get session ID or session status: " << e.what();
        }

        // Filter running sessions if needed
        if ((params.mRunning && status.mDDSSessionStatus == DDSSessionStatus::running) || (!params.mRunning)) {
            try {
                status.mAggregatedState = (topology != nullptr && session->mDDSTopo != nullptr)
                ?
                aggregateStateForPath(session->mDDSTopo.get(), topology->GetCurrentState(), "")
                :
                AggregatedState::Undefined;
            } catch (exception& e) {
                OLOG(warning, status.mPartitionID, 0) << "Failed to get an aggregated state: " << e.what();
            }
            result.mPartitions.push_back(status);
        }
    }
    result.mStatusCode = StatusCode::ok;
    result.mMsg = "Status done";
    result.mExecTime = params.mTimer.duration().count();
    return result;
}

void Controller::updateRestore()
{
    if (mRestoreId.empty()) {
        return;
    }

    RestoreData data;

    lock_guard<mutex> lock(mPartitionMtx);
    for (const auto& p : mPartitions) {
        const auto& session =  p.second.mSession;
        try {
            if (session->mDDSSession.IsRunning()) {
                data.mPartitions.push_back(RestorePartition(session->mPartitionID, to_string(session->mDDSSession.getSessionID())));
            }
        } catch (exception& e) {
            OLOG(warning, session->mPartitionID, 0) << "Failed to get session ID or session status: " << e.what();
        }
    }

    // Writing the file is locked by mPartitionMtx
    // This is done in order to prevent write failure in case of a parallel execution.
    RestoreFile(mRestoreId, mRestoreDir, data).write();
}

void Controller::updateHistory(const CommonParams& common, const std::string& sessionId)
{
    if (mHistoryDir.empty()) {
        return;
    }

    lock_guard<mutex> lock(mPartitionMtx);

    try {
        auto dir = filesystem::path(mHistoryDir);
        if (!filesystem::exists(dir) && !filesystem::create_directories(dir)) {
            throw runtime_error(toString("History: failed to create directory ", quoted(dir.string())));
        }

        filesystem::path filepath = dir / toString("odc_session_history.log");

        OLOG(info, common) << "Updating history file " << quoted(filepath.string()) << "...";

        stringstream ss;
        ss << getDateTime() << ", " << common.mPartitionID << ", " << sessionId << "\n";

        ofstream out;
        out.open(filepath, std::ios_base::app);
        out << ss.str() << flush;
    } catch (const std::exception& e) {
        OLOG(error) << "Failed to write history file " << quoted(toString(mHistoryDir, "/odc_session_history.log")) << ": " << e.what();
    }
}

RequestResult Controller::createRequestResult(const CommonParams& common, const Session& session, const Error& error, const string& msg, TopologyState&& topologyState, const std::string& rmsJobIDs, const std::unordered_set<std::string>& hosts)
{
    string sidStr = to_string(session.mDDSSession.getSessionID());
    StatusCode status = error.mCode ? StatusCode::error : StatusCode::ok;
    return RequestResult(status, msg, common.mTimer.duration().count(), error, common.mPartitionID, common.mRunNr, sidStr, std::move(topologyState), rmsJobIDs, hosts);
}

RequestResult Controller::createRequestResult(const CommonParams& common, const string& sessionId, const Error& error, const string& msg, TopologyState&& topologyState, const std::string& rmsJobIDs, const std::unordered_set<std::string>& hosts)
{
    StatusCode status = error.mCode ? StatusCode::error : StatusCode::ok;
    return RequestResult(status, msg, common.mTimer.duration().count(), error, common.mPartitionID, common.mRunNr, sessionId, std::move(topologyState), rmsJobIDs, hosts);
}

bool Controller::createDDSSession(const CommonParams& common, Session& session, Error& error)
{
    try {
        boost::uuids::uuid sessionID = session.mDDSSession.create();
        OLOG(info, common) << "DDS session created with session ID: " << to_string(sessionID);
        updateHistory(common, to_string(sessionID));
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::DDSCreateSessionFailed, toString("Failed to create a DDS session: ", e.what()));
        return false;
    }
    return true;
}

bool Controller::attachToDDSSession(const CommonParams& common, Session& session, Error& error, const string& sessionID)
{
    try {
        session.mDDSSession.attach(sessionID);
        OLOG(info, common) << "Attached to DDS session: " << sessionID;
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::DDSAttachToSessionFailed, toString("Failed to attach to a DDS session: ", e.what()));
        return false;
    }
    return true;
}

bool Controller::shutdownDDSSession(const CommonParams& common, Partition& partition, Error& error)
{
    try {
        partition.mTopology.reset();
        partition.mSession->mDDSTopo.reset();
        partition.mSession->mNinfo.clear();
        partition.mSession->mZoneInfo.clear();
        partition.mSession->mStandaloneTasks.clear();
        partition.mSession->mCollections.clear();
        partition.mSession->mAgentGroupInfo.clear();
        partition.mSession->mTopoFilePath.clear();
        partition.mSession->mExpendableTasks.clear();
        partition.mSession->mAgentInfo.clear();

        if (partition.mSession->mDDSSession.getSessionID() != boost::uuids::nil_uuid()) {
            if (partition.mSession->mDDSOnTaskDoneRequest) {
                partition.mSession->mDDSOnTaskDoneRequest->unsubscribeResponseCallback();
            }
            partition.mSession->mDDSSession.shutdown();
            if (partition.mSession->mDDSSession.getSessionID() == boost::uuids::nil_uuid()) {
                OLOG(info, common) << "DDS session has been shut down";
            } else {
                fillAndLogError(common, error, ErrorCode::DDSShutdownSessionFailed, "Failed to shut down DDS session");
                return false;
            }
        } else {
            OLOG(info, common) << "The session ID for the current DDS session is already zero. Not calling shutdown().";
        }
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::DDSShutdownSessionFailed, toString("Shutdown failed: ", e.what()));
        return false;
    }
    return true;
}

std::string Controller::getActiveDDSTopology(const CommonParams& common, Session& session, Error& error)
{
    using namespace dds::tools_api;
    try {
        dds::tools_api::SCommanderInfoRequest::response_t commanderInfo;
        stringstream ss;
        session.mDDSSession.syncSendRequest<SCommanderInfoRequest>(SCommanderInfoRequest::request_t(),
                                                                    commanderInfo,
                                                                    requestTimeout(common, "getActiveDDSTopology..syncSendRequest<SCommanderInfoRequest>"),
                                                                    &ss);
        OLOG(info, common) << ss.str();
        OLOG(debug, common) << "Commander info: " << commanderInfo;
        return commanderInfo.m_activeTopologyPath;
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Error getting DDS commander info: " << e;
        return "";
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::DDSCommanderInfoFailed, toString("Error getting DDS commander info: ", e.what()));
        return "";
    }
}

bool Controller::activateDDSTopology(const CommonParams& common, Session& session, Error& error, dds::tools_api::STopologyRequest::request_t::EUpdateType updateType)
{
    bool success = true;

    dds::tools_api::STopologyRequest::request_t topoInfo;
    topoInfo.m_topologyFile = session.mTopoFilePath;
    topoInfo.m_disableValidation = true;
    topoInfo.m_updateType = updateType;

    bool done = false;
    mutex mtx;
    condition_variable cv;

    dds::tools_api::STopologyRequest::ptr_t requestPtr = dds::tools_api::STopologyRequest::makeRequest(topoInfo);

    requestPtr->setMessageCallback([&success, &error, &common, this](const dds::tools_api::SMessageResponseData& msg) {
        if (msg.m_severity == dds::intercom_api::EMsgSeverity::error) {
            success = false;
            fillAndLogError(common, error, ErrorCode::DDSActivateTopologyFailed, toString("DDS Activate error: ", msg.m_msg));
        } else {
            // OLOG(debug, common) << "DDS Activate Message: " << msg.m_msg;
        }
    });

    requestPtr->setProgressCallback([&common](const dds::tools_api::SProgressResponseData& progress) {
        uint32_t completed{ progress.m_completed + progress.m_errors };
        if (completed == progress.m_total) {
            OLOG(debug, common) << "DDS Activated tasks (" << progress.m_completed << "), errors (" << progress.m_errors << "), total (" << progress.m_total << ")";
        }
    });

    requestPtr->setResponseCallback([&common, &session, &mtx](const dds::tools_api::STopologyResponseData& res) {
        OLOG(debug, common) << "DDS Activate Response: "
            << "agentID: " << res.m_agentID
            << "; slotID: " << res.m_slotID
            << "; taskID: " << res.m_taskID
            << "; collectionID: " << res.m_collectionID
            << "; host: " << res.m_host
            << "; path: " << res.m_path
            << "; workDir: " << res.m_wrkDir
            << "; activated: " << res.m_activated;

        // We are not interested in stopped tasks
        if (res.m_activated) {
            // response callbacks can be called in parallel - protect session access with a lock
            lock_guard<mutex> lock(mtx);
            std::string rmsJobId = session.mAgentGroupInfo.at(session.mAgentInfo.at(res.m_agentID).agentGroupInfoIndex).rmsJobID;
            session.mTaskDetails.emplace(res.m_taskID, TaskDetails{res.m_agentID, res.m_slotID, res.m_taskID, res.m_collectionID, res.m_path, res.m_host, res.m_wrkDir, rmsJobId});

            if (res.m_collectionID > 0) {
                if (session.mCollectionDetails.find(res.m_collectionID) == session.mCollectionDetails.end()) {
                    string path = res.m_path;
                    auto pos = path.rfind('/');
                    if (pos != string::npos) {
                        path.erase(pos);
                    }
                    session.mCollectionDetails.emplace(res.m_collectionID, CollectionDetails{res.m_agentID, res.m_collectionID, path, res.m_host, res.m_wrkDir, rmsJobId});
                    session.mRuntimeCollectionIndex.at(res.m_collectionID)->mRuntimeCollectionAgents[res.m_collectionID] = res.m_agentID;
                }
            }
        }
    });

    requestPtr->setDoneCallback([&mtx, &cv, &done]() {
        {
            lock_guard<mutex> lock(mtx);
            done = true;
        }
        cv.notify_all();
    });

    session.mDDSSession.sendRequest<dds::tools_api::STopologyRequest>(requestPtr);

    try {
        unique_lock<mutex> lock(mtx);
        cv.wait_for(lock, requestTimeout(common, "wait_for lock in activateDDSTopology"), [&done]{ return done; });

        if (!done) {
            success = false;
            fillAndLogError(common, error, ErrorCode::RequestTimeout, "Timed out waiting for topology activation");
            OLOG(error, common) << error;

            requestPtr->unsubscribeAll();

            auto agentInfo = getAgentInfo(common, session);
            OLOG(info, common) << agentInfo.size() << " DDS agents active:";
            for (const auto& ai : agentInfo) {
                OLOG(info, common)
                    << "  Agent ID: " << ai.m_agentID
                    << "; host: " << ai.m_host
                    << "; path: " << ai.m_DDSPath
                    << "; group: " << ai.m_groupName
                    << "; slots: " << ai.m_nSlots
                    << " (idle: " << ai.m_nIdleSlots
                    << ", executing: " << ai.m_nExecutingSlots << ").";
            }
        } else {
            // try {
            //     if (!session.mCollections.empty()) {
            //         OLOG(info, common) << "Collections:";
            //         for (const auto& [id, colInfo] : session.mCollections) {
            //             OLOG(info, common) << "  " << colInfo;
            //             for (const auto& [colId, agentId] : colInfo.mRuntimeCollectionAgents) {
            //                 OLOG(info, common) << "    runtime collection: id: " << colId << ", agent id: " << agentId;
            //             }
            //         }
            //     }
            // } catch (const exception& e) {
            //     fillAndLogError(common, error, ErrorCode::DDSActivateTopologyFailed, toString("Failed getting slot info: ", e.what()));
            // }
        }
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Error during topology activation: " << e;
        success = false;
    }

    // session.debug();

    OLOG(info, common) << "Topology " << quoted(session.mTopoFilePath) << ((success) ? " activated successfully" : " failed to activate");
    return success;
}

void Controller::extractRequirements(const CommonParams& common, Session& session)
{
    using namespace dds::topology_api;

    CTopology ddsTopo(session.mTopoFilePath);

    session.mNinfo.clear();
    session.mZoneInfo.clear();
    session.mStandaloneTasks.clear();
    session.mCollections.clear();
    session.mAgentGroupInfo.clear();

    OLOG(info, common) << "Extracting requirements from " << std::quoted(session.mTopoFilePath) << "...";

    auto taskIt = ddsTopo.getRuntimeTaskIterator();

    std::for_each(taskIt.first, taskIt.second, [&](const dds::topology_api::STopoRuntimeTask::FilterIterator_t::value_type& v) {
        auto& task = v.second;
        auto& topoTask = *(task.m_task);
        auto taskReqs = topoTask.getRequirements();

        for (const auto& tr : taskReqs) {
            if (tr->getRequirementType() == CTopoRequirement::EType::Custom) {
                if (strStartsWith(tr->getName(), "odc_expendable_")) {
                    if (tr->getValue() == "true") {
                        OLOG(debug, common) << "  Task '" << topoTask.getName() << "' (" << task.m_taskId << "), path: " << topoTask.getPath() << " [" << task.m_taskPath << "] is expendable";
                        session.mExpendableTasks.emplace(task.m_taskId);
                    } else if (tr->getValue() == "false") {
                        OLOG(debug, common) << "  Task '" << topoTask.getName() << "' (" << task.m_taskId << "), path: " << topoTask.getPath() << " [" << task.m_taskPath << "] is not expendable";
                    } else {
                        OLOG(error, common) << "  Task '" << topoTask.getName() << "' (" << task.m_taskId << "), path: " << topoTask.getPath() << " [" << task.m_taskPath << "] has 'odc_expendable_*' requirement defined, but with unknown value: " << quoted(tr->getValue());
                    }
                }
            }
        }
    });

    auto tasks = ddsTopo.getMainGroup()->getElementsByType(CTopoBase::EType::TASK);

    for (const auto& task : tasks) {
        CTopoTask::Ptr_t t = dynamic_pointer_cast<CTopoTask>(task);
        auto parent = t->getParent();

        // currently we are only interested in tasks which are inside a DDS collection or a DDS group
        if (parent->getName() == "main") {
            continue;
        }

        auto taskReqs = t->getRequirements();

        string zone;
        string agentGroup;
        string topoParent = parent->getName();
        int32_t n = t->getTotalCounter();

        for (const auto& tr : taskReqs) {
            if (tr->getRequirementType() == CTopoRequirement::EType::GroupName) {
                agentGroup = tr->getValue();
            } else if (tr->getRequirementType() == CTopoRequirement::EType::Custom) {
                if (strStartsWith(tr->getName(), "odc_zone_")) {
                    zone = tr->getValue();
                } else {
                    OLOG(debug, common) << "Unknown custom requirement found. name: " << quoted(tr->getName()) << ", value: " << quoted(tr->getValue());
                }
            }
        }

        session.mStandaloneTasks.emplace_back(TaskInfo{ t->getName(), zone, agentGroup, topoParent, n });
    }

    auto collections = ddsTopo.getMainGroup()->getElementsByType(CTopoBase::EType::COLLECTION);

    for (const auto& collection : collections) {
        CTopoCollection::Ptr_t c = dynamic_pointer_cast<CTopoCollection>(collection);
        auto colReqs = c->getRequirements();
        auto parent = c->getParent();

        string zone;
        string agentGroup;
        string topoParent = parent->getName();
        string topoPath = parent->getPath();
        int nCores = 0;
        int32_t n = c->getTotalCounter();
        int32_t nmin = -1;
        int32_t numTasks = c->getNofTasks();
        int32_t numTasksTotal = numTasks * n;

        for (const auto& cr : colReqs) {
            stringstream ss;
            ss << "  Collection '" << c->getName() << "' - requirement: " << cr->getName() << ", value: " << cr->getValue() << ", type: ";
            if (cr->getRequirementType() == CTopoRequirement::EType::GroupName) {
                ss << "GroupName";
                agentGroup = cr->getValue();
            } else if (cr->getRequirementType() == CTopoRequirement::EType::HostName) {
                ss << "HostName";
            } else if (cr->getRequirementType() == CTopoRequirement::EType::WnName) {
                ss << "WnName";
            } else if (cr->getRequirementType() == CTopoRequirement::EType::MaxInstancesPerHost) {
                ss << "MaxInstancesPerHost";
            } else if (cr->getRequirementType() == CTopoRequirement::EType::Custom) {
                ss << "Custom";
                if (strStartsWith(cr->getName(), "odc_ncores_")) {
                    nCores = stoi(cr->getValue());
                } else if (strStartsWith(cr->getName(), "odc_zone_")) {
                    zone = cr->getValue();
                } else if (strStartsWith(cr->getName(), "odc_nmin_")) {
                    // nMin is only relevant for collections that are in a group (which is not the main group)
                    if (parent->getType() == CTopoBase::EType::GROUP && parent->getName() != "main") {
                        nmin = stoll(cr->getValue());
                    } else {
                        // OLOG(info, common) << "collection " << c->getName() << " is not in a group, skipping nMin requirement";
                    }
                } else {
                    OLOG(debug, common) << "Unknown custom requirement found. name: " << quoted(cr->getName()) << ", value: " << quoted(cr->getValue());
                }
            } else {
                ss << ", type: Unknown";
            }
            OLOG(info, common) << ss.str();
        }

        // if the zone was not given explicitly, set it to the agent group
        if (zone.empty()) {
            zone = agentGroup;
        }

        // TODO: should n_current be set to 0 and increased as collections are launched instead?
        session.mCollections[c->getName()] = CollectionInfo{
            c->getName(),
            zone,
            agentGroup,
            topoParent,
            topoPath,
            n,
            n,
            nmin,
            nCores,
            numTasks,
            numTasksTotal,
            std::unordered_map<DDSCollectionId, uint64_t>(),
            std::unordered_set<DDSCollectionId>()};

        auto agiIt = std::find_if(session.mAgentGroupInfo.begin(), session.mAgentGroupInfo.end(), [agentGroup](const AgentGroupInfo& agi) {
            return agi.name == agentGroup;
        });
        if (agiIt == session.mAgentGroupInfo.end()) {
            session.mAgentGroupInfo.emplace_back(AgentGroupInfo{ agentGroup, zone, "unknown", n, nmin, numTasks, nCores });
        } else {
            agiIt->numAgents += n;
            agiIt->numSlots += numTasks;
            agiIt->zone = zone;
            agiIt->minAgents = nmin;
            agiIt->numCores = nCores;
        }

        if (!agentGroup.empty()) {
            auto nIt = session.mNinfo.find(c->getName());
            if (nIt == session.mNinfo.end()) {
                session.mNinfo.try_emplace(c->getName(), CollectionNInfo{ n, n, nmin, agentGroup });
            } else {
                // OLOG(info, common) << "collection " << c->getName() << " is already in the mNinfo";
            }
        }

        if (!agentGroup.empty() && !zone.empty()) {
            auto ziIt = session.mZoneInfo.find(zone);
            if (ziIt == session.mZoneInfo.end()) {
                session.mZoneInfo.try_emplace(zone, std::vector<ZoneGroup>{ ZoneGroup{n, nCores, agentGroup} });
            } else {
                ziIt->second.emplace_back(ZoneGroup{n, nCores, agentGroup});
            }
        }
    }

    auto colIt = ddsTopo.getRuntimeCollectionIterator();

    std::for_each(colIt.first, colIt.second, [&](const dds::topology_api::STopoRuntimeCollection::FilterIterator_t::value_type& v) {
        auto& col = v.second;
        auto& topoCol = *(col.m_collection);
        // OLOG(info, common) << "Runtime collection ID: " << col.m_collectionId << " is " << quoted(col.m_collectionPath) << " from " << topoCol.getName();
        session.mCollections.at(topoCol.getName()).mRuntimeCollectionAgents.emplace(col.m_collectionId, 0);
        session.mRuntimeCollectionIndex.emplace(col.m_collectionId, &(session.mCollections.at(topoCol.getName())));
    });

    if (!session.mZoneInfo.empty()) {
        OLOG(info, common) << "Zones from the topology:";
        for (const auto& z : session.mZoneInfo) {
            OLOG(info, common) << "  " << quoted(z.first) << ":";
            for (const auto& zi : z.second) {
                OLOG(info, common) << "    n: " << zi.n << ", nCores: " << zi.nCores << ", agentGroup: " << zi.agentGroup;
            }
        }
    }

    if (!session.mNinfo.empty()) {
        OLOG(info, common) << "N info:";
        for (const auto& [collection, nmin] : session.mNinfo) {
            OLOG(info, common) << "  collection: " << collection << ", " << nmin;
        }
    }

    if (!session.mCollections.empty()) {
        OLOG(info, common) << "Collections:";
        for (const auto& col : session.mCollections) {
            OLOG(info, common) << "  " << col.second;
            for (const auto& rc : col.second.mRuntimeCollectionAgents) {
                OLOG(info, common) << "    runtime collection: id: " << rc.first << " agent id: " << rc.second << " (0 if not yet known)";
            }
        }
    }

    if (!session.mStandaloneTasks.empty()) {
        OLOG(info, common) << "Tasks (outside of collections):";
        for (const auto& task : session.mStandaloneTasks) {
            OLOG(info, common) << "  " << task;
        }
    }

    OLOG(info, common) << "Agent groups:";
    for (const auto& agi : session.mAgentGroupInfo) {
        OLOG(info, common) << "  " << agi;
    }
}

bool Controller::createDDSTopology(const CommonParams& common, Session& session, Error& error)
{
    using namespace dds::topology_api;
    try {
        session.mDDSTopo = make_unique<CTopology>(session.mTopoFilePath);
        OLOG(info, common) << "DDS CTopology for " << quoted(session.mTopoFilePath) << " created successfully";
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::DDSCreateTopologyFailed, toString("Failed to initialize DDS topology: ", e.what()));
        return false;
    }
    return true;
}

bool Controller::createTopology(const CommonParams& common, Partition& partition, Error& error)
{
    try {
        partition.mTopology = make_unique<Topology>(*(partition.mSession->mDDSTopo), *(partition.mSession), false);
    } catch (exception& e) {
        partition.mTopology = nullptr;
        fillAndLogError(common, error, ErrorCode::FairMQCreateTopologyFailed, toString("Failed to initialize FairMQ topology: ", e.what()));
    }
    return partition.mTopology != nullptr;
}

bool Controller::resetTopology(Partition& partition)
{
    partition.mTopology.reset();
    return true;
}

bool Controller::changeState(const CommonParams& common, Partition& partition, Error& error, const string& path, TopoTransition transition, TopologyState& topologyState)
{
    if (partition.mTopology == nullptr) {
        fillAndLogError(common, error, ErrorCode::FairMQChangeStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    OLOG(info, common) << "Requesting transition " << toString(transition) << " for path " << quoted(path);

    auto it = gExpectedState.find(transition);
    DeviceState expState{ it != gExpectedState.end() ? it->second : DeviceState::Undefined };
    if (expState == DeviceState::Undefined) {
        fillAndLogError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Unexpected FairMQ transition ", transition));
        return false;
    }

    bool success = true;

    try {
        auto [errorCode, topoState] = partition.mTopology->ChangeState(transition, path, requestTimeout(common, toString("ChangeState(", transition, ")")));

        success = !errorCode;
        if (!success) {
            stateSummaryOnFailure(common, *(partition.mSession), partition.mTopology->GetCurrentState(), expState);
            switch (static_cast<ErrorCode>(errorCode.value())) {
                case ErrorCode::OperationTimeout:
                    fillAndLogFatalError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for ", transition, " transition"));
                    break;
                default:
                    fillAndLogFatalError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", errorCode.message()));
                    break;
            }
        }

        TopoStateByCollection collectionMap = GroupByCollectionId(topoState);
        if (topologyState.detailed.has_value()) {
            partition.mSession->fillDetailedState(topoState, collectionMap, topologyState.detailed.value());
        }

        topologyState.aggregated = AggregateState(topoState);
        if (success) {
            OLOG(info, common) << "State changed to " << topologyState.aggregated << " via " << transition << " transition";
        }

        printStateStats(common, topoState, collectionMap, false);
    } catch (Error& e) {
        error = e;
        stateSummaryOnFailure(common, *(partition.mSession), partition.mTopology->GetCurrentState(), expState);
        OLOG(fatal, common) << "Change state failed: " << e;
    } catch (exception& e) {
        stateSummaryOnFailure(common, *(partition.mSession), partition.mTopology->GetCurrentState(), expState);
        fillAndLogFatalError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", e.what()));
        success = false;
    }

    return success;
}

bool Controller::waitForState(const CommonParams& common, Partition& partition, Error& error, const string& path, DeviceState expState)
{
    if (partition.mTopology == nullptr) {
        fillAndLogError(common, error, ErrorCode::FairMQWaitForStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    OLOG(info, common) << "Waiting for the topology to reach " << expState << " state.";

    bool success = false;

    try {
        auto [errorCode, failedDevices] = partition.mTopology->WaitForState(DeviceState::Undefined, expState, path, requestTimeout(common, toString("WaitForState(", expState, ")")));

        success = !errorCode;
        if (!success) {
            stateSummaryOnFailure(common, *(partition.mSession), partition.mTopology->GetCurrentState(), expState);
            switch (static_cast<ErrorCode>(errorCode.value())) {
                case ErrorCode::OperationTimeout:
                    fillAndLogError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for ", expState, " state"));
                    break;
                default:
                    fillAndLogError(common, error, ErrorCode::FairMQWaitForStateFailed, toString("Failed waiting for ", expState, " state: ", errorCode.message()));
                    break;
            }
        } else {
            OLOG(info, common) << "Topology state is now " << expState;
        }
    } catch (Error& e) {
        error = e;
        stateSummaryOnFailure(common, *(partition.mSession), partition.mTopology->GetCurrentState(), expState);
        OLOG(fatal, common) << "Wait for state failed: " << e;
    } catch (exception& e) {
        stateSummaryOnFailure(common, *(partition.mSession), partition.mTopology->GetCurrentState(), expState);
        fillAndLogError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Wait for state failed: ", e.what()));
        success = false;
    }

    return success;
}

bool Controller::changeStateConfigure(const CommonParams& common, Partition& partition, Error& error, const string& path, TopologyState& topologyState)
{
    return changeState(common, partition, error, path, TopoTransition::InitDevice,   topologyState)
        && changeState(common, partition, error, path, TopoTransition::CompleteInit, topologyState)
        && changeState(common, partition, error, path, TopoTransition::Bind,         topologyState)
        && changeState(common, partition, error, path, TopoTransition::Connect,      topologyState)
        && changeState(common, partition, error, path, TopoTransition::InitTask,     topologyState);
}

bool Controller::changeStateReset(const CommonParams& common, Partition& partition, Error& error, const string& path, TopologyState& topologyState)
{
    return changeState(common, partition, error, path, TopoTransition::ResetTask,   topologyState)
        && changeState(common, partition, error, path, TopoTransition::ResetDevice, topologyState);
}

void Controller::getState(const CommonParams& common, Partition& partition, Error& error, const string& path, TopologyState& topologyState)
{
    if (partition.mTopology == nullptr) {
        topologyState.aggregated = AggregatedState::Undefined;
        return;
    }

    auto const topoState = partition.mTopology->GetCurrentState();

    try {
        topologyState.aggregated = aggregateStateForPath(partition.mSession->mDDSTopo.get(), topoState, path);
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::FairMQGetStateFailed, toString("Get state failed: ", e.what()));
    }
    TopoStateByCollection collectionMap = GroupByCollectionId(topoState);
    if (topologyState.detailed.has_value()) {
        partition.mSession->fillDetailedState(topoState, collectionMap, topologyState.detailed.value());
    }

    printStateStats(common, topoState, collectionMap, true);
}

bool Controller::setProperties(const CommonParams& common, Partition& partition, Error& error, const string& path, const SetPropertiesParams::Props& props, TopologyState& topologyState)
{
    if (partition.mTopology == nullptr) {
        fillAndLogError(common, error, ErrorCode::FairMQSetPropertiesFailed, "FairMQ topology is not initialized");
        return false;
    }

    try {
        auto [errorCode, failedDevices] = partition.mTopology->SetProperties(props, path, requestTimeout(common, "SetProperties"));
        if (!errorCode) {
            OLOG(info, common) << "Set property finished successfully";
        } else {
            size_t count = 1;
            OLOG(error, common) << "Following devices failed to set properties: ";
            for (auto taskId : failedDevices) {
                TaskDetails& taskDetails = partition.mSession->getTaskDetails(taskId);
                OLOG(error, common) << "  [" << count++ << "] " << taskDetails;
            }
            switch (static_cast<ErrorCode>(errorCode.value())) {
                case ErrorCode::OperationTimeout:
                    fillAndLogError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for set property: ", errorCode.message()));
                    break;
                default:
                    fillAndLogError(common, error, ErrorCode::FairMQSetPropertiesFailed, toString("Set property error message: ", errorCode.message()));
                    break;
            }
        }

        topologyState.aggregated = AggregateState(partition.mTopology->GetCurrentState());
    } catch (Error& e) {
        error = e;
        OLOG(error, common) << "Set properties failed: " << e;
    } catch (exception& e) {
        fillAndLogError(common, error, ErrorCode::FairMQSetPropertiesFailed, toString("Set properties failed: ", e.what()));
    }

    return !error.mCode;
}

AggregatedState Controller::aggregateStateForPath(const dds::topology_api::CTopology* ddsTopo, const TopoState& topoState, const string& path)
{
    if (path.empty()) {
        return AggregateState(topoState);
    }

    if (ddsTopo == nullptr) {
        throw runtime_error("DDS topology is not initialized");
    }

    try {
        // Check if path points to a single task
        // If task is found than return it's state as an aggregated topology state

        // Throws if task not found for path
        const auto& task{ ddsTopo->getRuntimeTask(path) };
        auto it{ find_if(topoState.cbegin(), topoState.cend(), [&](const TopoState::value_type& v) { return v.taskId == task.m_taskId; }) };
        if (it != topoState.cend())
            return static_cast<AggregatedState>(it->state);

        throw runtime_error("Device not found for path " + path);
    } catch (exception& /* e */) {
        // In case of exception check that path contains multiple tasks

        // Collect all task IDs to a set for fast search
        set<dds::topology_api::Id_t> taskIds;
        auto it{ ddsTopo->getRuntimeTaskIteratorMatchingPath(path) };
        for_each(it.first, it.second, [&](const dds::topology_api::STopoRuntimeTask::FilterIterator_t::value_type& v) {
            taskIds.insert(v.second.m_taskId);
        });

        if (taskIds.empty()) {
            throw runtime_error("No tasks found matching the path " + path);
        }

        // Find a state of a first task
        auto firstIt{ find_if(topoState.cbegin(), topoState.cend(), [&](const TopoState::value_type& v) {
            return v.taskId == *(taskIds.begin());
        }) };
        if (firstIt == topoState.cend()) {
            throw runtime_error("No states found for path " + path);
        }

        // Check that all selected devices have the same state
        AggregatedState first{ static_cast<AggregatedState>(firstIt->state) };
        if (all_of(topoState.cbegin(), topoState.cend(), [&](const TopoState::value_type& v) {
                return (taskIds.count(v.taskId) > 0) ? v.state == first : true;
            })) {
            return first;
        }

        return AggregatedState::Mixed;
    }
}

void Controller::fillAndLogError(const CommonParams& common, Error& error, ErrorCode errorCode, const string& msg)
{
    error.mCode = MakeErrorCode(errorCode);
    error.mDetails = msg;
    OLOG(error, common) << error.mDetails;
}

void Controller::fillAndLogFatalError(const CommonParams& common, Error& error, ErrorCode errorCode, const string& msg)
{
    error.mCode = MakeErrorCode(errorCode);
    error.mDetails = msg;
    OLOG(fatal, common) << error.mDetails;
}

void Controller::logLineWarningOrDetectedSev(const CommonParams& common, const string& line)
{
    string lowerCaseLine = line;
    std::transform(lowerCaseLine.begin(), lowerCaseLine.end(), lowerCaseLine.begin(), [](unsigned char c){ return std::tolower(c); });

    if (lowerCaseLine.find("fatal") != string::npos) {
        OLOG(fatal, common) << line;
    } else if (lowerCaseLine.find("error") != string::npos) {
        OLOG(error, common) << line;
    } else {
        OLOG(warning, common) << line;
    }
}

void Controller::logFatalLineByLine(const CommonParams& common, const string& msg)
{
    stringstream ss(msg);
    string line;
    while (getline(ss, line, '\n')) {
        logLineWarningOrDetectedSev(common, line);
    }
}

Partition& Controller::acquirePartition(const CommonParams& common)
{
    try {
        lock_guard<mutex> lock(mPartitionMtx);
        auto it = mPartitions.find(common.mPartitionID);
        if (it == mPartitions.end()) {
            auto [partitionIt, inserted] = mPartitions.try_emplace(common.mPartitionID, Partition(common.mPartitionID));
            try {
                partitionIt->second.mSession = make_unique<Session>();
                partitionIt->second.mSession->mPartitionID = common.mPartitionID;
                // OLOG(debug, common) << "Created partition " << quoted(common.mPartitionID);
                return partitionIt->second;
            } catch (const exception& e) {
                OLOG(error, common) << "Error creating session data for the partition: " << e.what();
                mPartitions.erase(partitionIt);
                throw;
            }
        }
        // OLOG(debug, common) << "Found partition " << quoted(common.mPartitionID);
        return it->second;
    } catch (const exception& e) {
        OLOG(error, common) << "Error acquiring partition: " << e.what();
        throw;
    }
}

void Controller::removePartition(const CommonParams& common)
{
    lock_guard<mutex> lock(mPartitionMtx);
    size_t numRemoved = mPartitions.erase(common.mPartitionID);
    if (numRemoved == 1) {
        OLOG(debug, common) << "Removed Partition " << quoted(common.mPartitionID);
    } else {
        OLOG(debug, common) << "Found no partition " << quoted(common.mPartitionID);
    }
}

void Controller::stateSummaryOnFailure(const CommonParams& common, Session& session, const TopoState& topoState, DeviceState expectedState)
{
    std::vector<CollectionDetails*> failedCollections;
    try {
        size_t numFailedTasks = 0;
        for (const auto& status : topoState) {
            if (status.state == expectedState || status.ignored) {
                continue;
            }

            if (++numFailedTasks == 1) {
                OLOG(error, common) << "Following devices failed to transition to " << expectedState << " state:";
            }

            TaskDetails& taskDetails = session.getTaskDetails(status.taskId);
            OLOG(error, common) << "  [" << numFailedTasks << "] "
                                << taskDetails
                                << ", state: " << status.state
                                << ", previous state: " << status.lastState
                                << ", subscribed: " << boolalpha << status.subscribedToStateChanges
                                << ", ignored: " << boolalpha << status.ignored
                                << ", expendable: " << boolalpha << status.expendable;

            // if task is in a collection, consider it failed too
            if (taskDetails.mCollectionID != 0) {
                // check if the failed collection is in the list of failed collections
                auto it = find_if(failedCollections.cbegin(), failedCollections.cend(), [&](const auto& ci) {
                    return ci->mCollectionID == taskDetails.mCollectionID;
                });
                // otherwise add it
                if (it == failedCollections.cend()) {
                    CollectionDetails& collectionDetails = session.getCollectionDetails(taskDetails.mCollectionID);
                    failedCollections.push_back(&collectionDetails);
                }
            }
        }

        if (!failedCollections.empty()) {
            int index = 1;
            OLOG(error, common) << "Following collections failed to transition to " << expectedState << " state:";
            for (const auto& col : failedCollections) {
                OLOG(error, common) << "  [" << index++ << "] " << *col;
            }
        }

        size_t numOkTasks = session.mTaskDetails.size() - numFailedTasks;
        size_t numOkCollections = session.mCollectionDetails.size() - failedCollections.size();
        OLOG(error, common) << "Summary after transitioning to " << expectedState << " state:";
        OLOG(error, common) << "  [tasks] total: " << session.mTaskDetails.size() << ", successful: " << numOkTasks << ", failed: " << numFailedTasks;
        OLOG(error, common) << "  [collections] total: " << session.mCollectionDetails.size() << ", successful: " << numOkCollections << ", failed: " << failedCollections.size();
    } catch (const exception& e) {
        OLOG(error, common) << "State summary error: " << e.what();
    }
}

// void Controller::ShutdownDDSAgent(const CommonParams& common, Session& session, uint64_t agentID)
// {
//     try {
//         size_t currentSlotCount = session.mTotalSlots;
//         size_t numSlotsToRemove = session.mAgentIngo.at(agentID).numSlots;
//         size_t expectedNumSlots = session.mTotalSlots - numSlotsToRemove;
//         OLOG(info, common) << "Current number of slots: " << session.mTotalSlots << ", expecting to reduce to " << expectedNumSlots;

//         using namespace dds::tools_api;
//         OLOG(info, common) << "Sending shutdown signal to agent " << agentID;;
//         SAgentCommandRequest::request_t agentCmd;
//         agentCmd.m_commandType = SAgentCommandRequestData::EAgentCommandType::shutDownByID;
//         agentCmd.m_arg1 = agentID;
//         session.mDDSSession.syncSendRequest<SAgentCommandRequest>(agentCmd, requestTimeout(common, "ShutdownDDSAgent..syncSendRequest<SAgentCommandRequest>"));

//         // TODO: notification on agent shutdown in development in DDS
//         currentSlotCount = getNumSlots(common, session);
//         OLOG(info, common) << "Current number of slots: " << currentSlotCount;

//         if (currentSlotCount != expectedNumSlots) {
//             int64_t secondsLeft = requestTimeout(common, "ShutdownDDSAgent..measure remaining time").count();
//             if (secondsLeft > 0) {
//                 int64_t maxAttempts = (secondsLeft * 1000) / 50;
//                 while (currentSlotCount != expectedNumSlots && maxAttempts > 0) {
//                     this_thread::sleep_for(chrono::milliseconds(50));
//                     currentSlotCount = getNumSlots(common, session);
//                     // OLOG(info, common) << "Current number of slots: " << currentSlotCount;
//                     --maxAttempts;
//                 }
//             }
//         }
//         if (currentSlotCount != expectedNumSlots) {
//             OLOG(warning, common) << "Could not reduce the number of slots to " << expectedNumSlots << ", current count is: " << currentSlotCount;
//         } else {
//             OLOG(info, common) << "Successfully reduced number of slots to " << currentSlotCount;
//         }
//         session.mTotalSlots = currentSlotCount;
//     } catch (Error& e) {
//         OLOG(error, common) << "Agent Shutdown failed: " << e;
//     } catch (exception& e) {
//         OLOG(error, common) << "Failed updating nubmer of slots: " << e.what();
//     }
// }

dds::tools_api::SAgentInfoRequest::responseVector_t Controller::getAgentInfo(const CommonParams& common, Session& session) const
{
    using namespace dds::tools_api;
    SAgentInfoRequest::responseVector_t agentInfo;
    SAgentInfoRequest::request_t infoRequest;
    SAgentInfoRequest::ptr_t requestPtr = SAgentInfoRequest::makeRequest(infoRequest);

    bool done = false;
    mutex mtx;
    condition_variable cv;

    requestPtr->setResponseCallback([&agentInfo](const SAgentInfoResponseData& response) {
        agentInfo.push_back(response);
    });

    requestPtr->setMessageCallback([&common](const SMessageResponseData& msg) {
        if (msg.m_severity == dds::intercom_api::EMsgSeverity::error) {
            OLOG(error, common) << "DDS Failed to collect agent info agents: " << msg.m_msg;
        } else {
            OLOG(info, common) << "DDS Server reports: " << msg.m_msg;
        }
    });

    requestPtr->setDoneCallback([&mtx, &cv, &done]() {
        {
            lock_guard<mutex> lock(mtx);
            done = true;
        }
        cv.notify_all();
    });

    session.mDDSSession.sendRequest<SAgentInfoRequest>(requestPtr);

    try {
        unique_lock<mutex> lock(mtx);
        cv.wait_for(lock, requestTimeout(common, "wait_for lock in getAgentInfo"), [&done]{ return done; });

        if (!done) {
            OLOG(error, common) << "Timed out waiting for DDS agent info";

            requestPtr->unsubscribeAll();
        } else {
            // OLOG(info, common) << "Agent submission done successfully";
        }
    } catch (Error& e) {
        OLOG(error, common) << "Agent submission error: " << e;
    }

    // session.mDDSSession.syncSendRequest<SAgentInfoRequest>(SAgentInfoRequest::request_t(), agentInfo, requestTimeout(common, "getAgentInfo..syncSendRequest<SAgentInfoRequest>"));
    return agentInfo;
}

uint32_t Controller::getNumSlots(const CommonParams& common, Session& session) const
{
    using namespace dds::tools_api;
    SAgentCountRequest::response_t agentCountInfo;
    session.mDDSSession.syncSendRequest<SAgentCountRequest>(SAgentCountRequest::request_t(), agentCountInfo, requestTimeout(common, "getNumSlots..syncSendRequest<SAgentCountRequest>"));
    return agentCountInfo.m_activeSlotsCount;
}

string Controller::topoFilepath(const CommonParams& common, const string& topologyFile, const string& topologyContent, const string& topologyScript)
{
    int count{ (topologyFile.empty() ? 0 : 1) + (topologyContent.empty() ? 0 : 1) + (topologyScript.empty() ? 0 : 1) };
    if (count != 1) {
        throw runtime_error("Either topology filepath, content or script has to be set");
    }
    if (!topologyFile.empty()) {
        return topologyFile;
    }

    string content{ topologyContent };

    // Execute topology script if needed
    if (!topologyScript.empty()) {
        string out;
        string err;
        int exitCode = EXIT_SUCCESS;
        OLOG(info, common) << "Executing topology generation script: " << topologyScript;
        std::vector<std::pair<std::string, std::string>> extraEnv;
        extraEnv.emplace_back(std::make_pair("ODC_TOPO_GEN_CMD", topologyScript));
        execute(topologyScript, requestTimeout(common, "topoFilepath..execute"), &out, &err, &exitCode, extraEnv);

        const size_t shortSize = 75;
        string shortSuffix;
        string shortOut = out.substr(0, shortSize);
        if (out.length() > shortSize) {
            shortSuffix = " [...]";
        }

        if (exitCode != EXIT_SUCCESS) {
            OLOG(fatal, common) << "Topology generation script failed with exit code: " << exitCode;
            logFatalLineByLine(common, toString(", stderr:\n", quoted(err), ",\nstdout:\n", quoted(shortOut), shortSuffix));
            throw runtime_error(toString("Topology generation script failed with exit code: ", exitCode, ", stderr: ", quoted(err)));
        }

        if (out.empty()) {
            OLOG(fatal, common) << "Topology generation script produced no output. Check the script for errors:";
            logFatalLineByLine(common, topologyScript);
            throw runtime_error("Topology generation script produced no output. Check the script for errors.");
        }

        OLOG(info, common) << "Topology generation script successfull. stderr: " << quoted(err) << ", stdout: " << quoted(shortOut) << shortSuffix;

        content = out;
    }

    // Create temp topology file with `content`
    const bfs::path tmpPath{ bfs::temp_directory_path() / bfs::unique_path() };
    bfs::create_directories(tmpPath);
    const bfs::path filepath{ tmpPath / "topology.xml" };
    ofstream file(filepath.string());
    if (!file.is_open()) {
        throw runtime_error(toString("Failed to create temporary topology file ", quoted(filepath.string())));
    }
    file << content;
    OLOG(info, common) << "Temp topology file " << quoted(filepath.string()) << " created successfully";
    return filepath.string();
}

void Controller::registerResourcePlugins(const DDSSubmit::PluginMap& pluginMap)
{
    for (const auto& v : pluginMap) {
        mSubmit.registerPlugin(v.first, v.second);
    }
}

void Controller::restore(const string& id, const string& dir)
{
    mRestoreId = id;
    mRestoreDir = dir;

    OLOG(info) << "Restoring sessions for " << quoted(id);

    RestoreData data;
    {
        lock_guard<mutex> lock(mPartitionMtx);
        data = RestoreFile(id, dir).read();
    }

    OLOG(info) << "Found " << data.mPartitions.size() << " partitions to restore";

    for (const auto& v : data.mPartitions) {
        OLOG(info, v.mPartitionID, 0) << "Restoring (" << quoted(v.mPartitionID) << "/" << quoted(v.mDDSSessionId) << ")";
        auto result{ execInitialize(CommonParams(v.mPartitionID, 0, 0), InitializeParams(v.mDDSSessionId)) };
        if (result.mError.mCode) {
            OLOG(info, v.mPartitionID, 0) << "Failed to attach to the session for partition " << quoted(v.mPartitionID) << ", session " << quoted(v.mDDSSessionId);
        } else {
            OLOG(info, v.mPartitionID, 0) << "Successfully attached to the session (" << quoted(v.mPartitionID) << "/" << quoted(v.mDDSSessionId) << ")";
        }
    }
}

void Controller::setZoneCfgs(const std::vector<std::string>& zonesStr)
{
    for (const auto& z : zonesStr) {
        std::vector<std::string> zoneCfg;
        boost::algorithm::split(zoneCfg, z, boost::algorithm::is_any_of(":"));
        if (zoneCfg.size() != 3) {
            throw std::runtime_error(odc::core::toString("Provided zones configuration has incorrect format. Expected <name>:<cfgFilePath>:<envFilePath>. Received: ", z));
        }
        mZoneCfgs[zoneCfg.at(0)] = ZoneConfig{ zoneCfg.at(1), zoneCfg.at(2) };
    }
}

void Controller::printStateStats(const CommonParams& common, const TopoState& topoState, const TopoStateByCollection& collectionMap, bool debugLog)
{
    std::map<DeviceState, uint64_t> taskStateCounts;
    std::map<AggregatedState, uint64_t> collectionStateCounts;

    for (const auto& ds : topoState) {
        if (taskStateCounts.find(ds.state) == taskStateCounts.end()) {
            taskStateCounts[ds.state] = 0;
        }
        taskStateCounts[ds.state]++;
    }

    for (const auto& [collectionId, states] : collectionMap) {
        AggregatedState collectionState = AggregateState(states);
        if (collectionStateCounts.find(collectionState) == collectionStateCounts.end()) {
            collectionStateCounts[collectionState] = 0;
        }
        collectionStateCounts[collectionState]++;
    }

    stringstream ss;
    ss << "Device states:";
    for (const auto& [state, count] : taskStateCounts) {
        ss << " " << fair::mq::GetStateName(state) << " (" << count << "/" << topoState.size() << ")";
    }
    if (debugLog) {
        OLOG(debug, common) << ss.str();
    } else {
        OLOG(info, common) << ss.str();
    }
    ss.str("");
    ss.clear();
    ss << "Collection states:";
    for (const auto& [state, count] : collectionStateCounts) {
        ss << " " << GetAggregatedStateName(state) << " (" << count << "/" << collectionMap.size() << ")";
    }
    if (debugLog) {
        OLOG(debug, common) << ss.str();
    } else {
        OLOG(info, common) << ss.str();
    }
}
