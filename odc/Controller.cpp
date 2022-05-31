/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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
#include <odc/Timer.h>
#include <odc/Topology.h>

#include <dds/dds.h>
#include <dds/TopoVars.h>
#include <dds/Tools.h>
#include <dds/Topology.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <algorithm>

using namespace odc;
using namespace odc::core;
using namespace std;
namespace bfs = boost::filesystem;

RequestResult Controller::execInitialize(const CommonParams& common, const InitializeParams& params)
{
    Timer timer;

    Error error;
    if (params.mDDSSessionID.empty()) {
        // Create new DDS session
        // Shutdown DDS session if it is running already
        shutdownDDSSession(common, error)
            && createDDSSession(common, error)
            && subscribeToDDSSession(common, error);
    } else {
        // Attach to an existing DDS session
        // Shutdown DDS session if it is running already
        bool success = attachToDDSSession(common, error, params.mDDSSessionID)
            && subscribeToDDSSession(common, error);

        // Request current active topology, if any
        // If topology is active, create DDS and FairMQ topology
        if (success) {
            dds::tools_api::SCommanderInfoRequest::response_t commanderInfo;
            requestCommanderInfo(common, error, commanderInfo)
                && !commanderInfo.m_activeTopologyPath.empty()
                && createDDSTopology(common, error, commanderInfo.m_activeTopologyPath)
                && createTopology(common, error, commanderInfo.m_activeTopologyPath);
        }
    }
    execRequestTrigger("Initialize", common);
    updateRestore();
    return createRequestResult(common, error, "Initialize done", timer.duration(), AggregatedState::Undefined);
}

RequestResult Controller::execSubmit(const CommonParams& common, const SubmitParams& params)
{
    Timer timer;

    Error error;
    auto& session = getOrCreateSession(common);
    if (!session.mDDSSession->IsRunning()) {
        fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, "DDS session is not running. Use Init or Run to start the session.");
    }

    // Get DDS submit parameters from ODC resource plugin
    vector<DDSSubmit::Params> ddsParams;
    if (!error.mCode) {
        try {
            ddsParams = mSubmit.makeParams(params.mPlugin, params.mResources, common.mPartitionID, common.mRunNr);
        } catch (exception& e) {
            fillError(common, error, ErrorCode::ResourcePluginFailed, toString("Resource plugin failed: ", e.what()));
        }
    }

    if (!error.mCode) {
        OLOG(info, common) << "Preparing to submit " << ddsParams.size() << " configurations";

        for (unsigned int i = 0; i < ddsParams.size(); ++i) {
            auto it = find_if(session.mNinfo.cbegin(), session.mNinfo.cend(), [&](const auto& tgi) {
                return tgi.second.agentGroup == ddsParams.at(i).mAgentGroup;
            });
            if (it != session.mNinfo.cend()) {
                ddsParams.at(i).mMinAgents = it->second.nMin;
            }
            OLOG(info, common) << "Submitting [" << i + 1 << "/" << ddsParams.size() << "]: " << ddsParams.at(i);

            if (submitDDSAgents(session, common, error, ddsParams.at(i))) {
                session.mTotalSlots += ddsParams.at(i).mNumAgents * ddsParams.at(i).mNumSlots;
            } else {
                OLOG(error, common) << "Submission failed";
                break;
            }
        }
        if (!error.mCode) {
            OLOG(info, common) << "Waiting for " << session.mTotalSlots << " slots...";
            if (waitForNumActiveSlots(session, common, error, session.mTotalSlots)) {
                OLOG(info, common) << "Done waiting for " << session.mTotalSlots << " slots.";
            }
        }
    }

    if (session.mDDSSession->IsRunning()) {
        map<string, uint32_t> agentCounts; // agent count sorted by their group name
        auto agentInfo = getAgentInfo(session, common);
        OLOG(info, common) << "Launched " << agentInfo.size() << " DDS agents:";
        for (const auto& ai : agentInfo) {
            agentCounts[ai.m_groupName]++;
            session.mAgentSlots[ai.m_agentID] = ai.m_nSlots;
            OLOG(info, common) << "Agent ID: " << ai.m_agentID
                               // << ", pid: " << ai.m_agentPid
                               << ", host: " << ai.m_host
                               << ", path: " << ai.m_DDSPath
                               << ", group: " << ai.m_groupName
                               // << ", index: " << ai.m_index
                               // << ", username: " << ai.m_username
                               << ", slots: " << ai.m_nSlots << " (idle: " << ai.m_nIdleSlots << ", executing: " << ai.m_nExecutingSlots << ").";
        }
        OLOG(info, common) << "Number of launched agents, sorted by group:";
        for (const auto& [groupName, count] : agentCounts) {
            OLOG(info, common) << groupName << ": " << count;
        }

        if (error.mCode) {
            attemptSubmitRecovery(session, ddsParams, agentCounts, error, common);
        }
    }

    execRequestTrigger("Submit", common);
    return createRequestResult(common, error, "Submit done", timer.duration(), AggregatedState::Undefined);
}

void Controller::attemptSubmitRecovery(Session& session,
                                       const vector<DDSSubmit::Params>& ddsParams,
                                       const map<string, uint32_t>& agentCounts,
                                       Error& error,
                                       const CommonParams& common)
{
    error = Error();
    for (const auto& p : ddsParams) {
        if (p.mNumAgents != agentCounts.at(p.mAgentGroup)) {
            // fail recovery if insufficient agents, and no nMin is defined
            if (p.mMinAgents == 0) {
                fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString(
                    "Number of agents (", agentCounts.at(p.mAgentGroup), ") for group '", p.mAgentGroup
                    , "' is less than requested (", p.mNumAgents, "), "
                    , "and no nMin is defined"));
                return;
            }
            // fail recovery if insufficient agents, and no nMin is defined
            if (agentCounts.at(p.mAgentGroup) < p.mMinAgents) {
                fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString(
                    "Number of agents (", agentCounts.at(p.mAgentGroup), ") for group '", p.mAgentGroup
                    , "' is less than requested (", p.mNumAgents, "), "
                    , "and nMin (", p.mMinAgents, ") is not satisfied"));
                return;
            }
            OLOG(info, common) << "Number of agents (" << agentCounts.at(p.mAgentGroup) << ") for group '" << p.mAgentGroup
                               << "' is less than requested (" << p.mNumAgents << "), "
                               << "but nMin (" << p.mMinAgents << ") is satisfied";
        }
    }
    if (!error.mCode) {
        session.mTotalSlots = getNumSlots(session, common);
        updateTopology(session, agentCounts, common);
        for (auto& tgi : session.mNinfo) {
            auto it = find_if(agentCounts.cbegin(), agentCounts.cend(), [&](const auto& ac) {
                return ac.first == tgi.second.agentGroup;
            });
            if (it != agentCounts.cend()) {
                tgi.second.nCurrent = it->second;
            }
        }
    }
}

void Controller::updateTopology(Session& session, const map<string, uint32_t>& agentCounts, const CommonParams& common)
{
    using namespace dds::topology_api;
    OLOG(info, common) << "Updating current topology file (" << session.mTopoFilePath << ") to reflect the reduced number of groups";

    CTopoCreator creator;
    creator.getMainGroup()->initFromXML(session.mTopoFilePath);
    auto groups = creator.getMainGroup()->getElementsByType(CTopoBase::EType::GROUP);
    for (const auto& group : groups) {
        CTopoGroup::Ptr_t g = dynamic_pointer_cast<CTopoGroup>(group);
        try {
            string agentGroup = session.mNinfo.at(g->getName()).agentGroup;
            g->setN(agentCounts.at(agentGroup));
        } catch (exception& e) {
            continue;
        }
    }

    string name("topo_" + session.mPartitionID + "_reduced.xml");
    const bfs::path tmpPath{ bfs::temp_directory_path() / bfs::unique_path() };
    bfs::create_directories(tmpPath);
    const bfs::path filepath{ tmpPath / name };
    creator.save(filepath.string());

    // re-add the nmin variables
    CTopoVars vars;
    vars.initFromXML(filepath.string());
    for (const auto& [groupName, groupInfo] : session.mNinfo) {
        string varName("odc_nmin_" + groupName);
        vars.add(varName, std::to_string(groupInfo.nMin));
    }
    vars.saveToXML(filepath.string());
    session.mTopoFilePath = filepath.string();

    OLOG(info, common) << "Saved updated topology file as " << session.mTopoFilePath;
}

RequestResult Controller::execActivate(const CommonParams& common, const ActivateParams& params)
{
    Timer timer;
    auto& session = getOrCreateSession(common);
    Error error{ checkSessionIsRunning(common, ErrorCode::DDSActivateTopologyFailed) };
    if (!error.mCode) {
        try {
            if (session.mTopoFilePath.empty()) {
                session.mTopoFilePath = topoFilepath(common, params.mDDSTopologyFile,
                                                             params.mDDSTopologyContent,
                                                             params.mDDSTopologyScript);
                extractNmin(common, session.mTopoFilePath);
            }
        } catch (exception& e) {
            fillFatalError(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
        }
        if (!error.mCode) {
            activateDDSTopology(common, error, session.mTopoFilePath, dds::tools_api::STopologyRequest::request_t::EUpdateType::ACTIVATE)
                && createDDSTopology(common, error, session.mTopoFilePath)
                && createTopology(common, error, session.mTopoFilePath)
                && waitForState(common, error, DeviceState::Idle, "");
        }
    }
    AggregatedState state{ !error.mCode ? AggregatedState::Idle : AggregatedState::Undefined };
    execRequestTrigger("Activate", common);
    return createRequestResult(common, error, "Activate done", timer.duration(), state);
}

RequestResult Controller::execRun(const CommonParams& common, const InitializeParams& initializeParams, const SubmitParams& submitParams, const ActivateParams& activateParams)
{
    Timer timer;
    // Run request doesn't support attachment to a DDS session.
    auto& session = getOrCreateSession(common);

    Error error;
    if (!initializeParams.mDDSSessionID.empty()) {
        error = Error(MakeErrorCode(ErrorCode::RequestNotSupported), "Attachment to a DDS session not supported");
    } else {
        error = execInitialize(common, initializeParams).mError;
        if (!error.mCode) {
            try {
                session.mTopoFilePath = topoFilepath(common, activateParams.mDDSTopologyFile,
                                                             activateParams.mDDSTopologyContent,
                                                             activateParams.mDDSTopologyScript);
                extractNmin(common, session.mTopoFilePath);
            } catch (exception& e) {
                fillFatalError(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
            }
            if (!error.mCode) {
                error = execSubmit(common, submitParams).mError;
                if (!error.mCode) {
                    error = execActivate(common, activateParams).mError;
                }
            }
        }
    }
    AggregatedState state{ !error.mCode ? AggregatedState::Idle : AggregatedState::Undefined };
    execRequestTrigger("Run", common);
    return createRequestResult(common, error, "Run done", timer.duration(), state);
}

RequestResult Controller::execUpdate(const CommonParams& common, const UpdateParams& params)
{
    Timer timer;
    auto& session = getOrCreateSession(common);
    AggregatedState state{ AggregatedState::Undefined };
    Error error;

    try {
        session.mTopoFilePath = topoFilepath(common, params.mDDSTopologyFile, params.mDDSTopologyContent, params.mDDSTopologyScript);
        extractNmin(common, session.mTopoFilePath);
    } catch (exception& e) {
        fillFatalError(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
    }

    if (!error.mCode) {
        changeStateReset(common, error, "", state)
            && resetTopology(common)
            && activateDDSTopology(common, error, session.mTopoFilePath, dds::tools_api::STopologyRequest::request_t::EUpdateType::UPDATE)
            && createDDSTopology(common, error, session.mTopoFilePath)
            && createTopology(common, error, session.mTopoFilePath)
            && waitForState(common, error, DeviceState::Idle, "")
            && changeStateConfigure(common, error, "", state);
    }
    execRequestTrigger("Update", common);
    return createRequestResult(common, error, "Update done", timer.duration(), state);
}

RequestResult Controller::execShutdown(const CommonParams& common)
{
    Timer timer;
    Error error;

    auto& session = getOrCreateSession(common);
    // grab session id before shutting down the session, to return it in the reply
    string sidStr{ to_string(session.mDDSSession->getSessionID()) };

    shutdownDDSSession(common, error);
    removeSession(common);
    execRequestTrigger("Shutdown", common);
    auto result = createRequestResult(common, error, "Shutdown done", timer.duration(), AggregatedState::Undefined);
    result.mDDSSessionID = sidStr;
    return result;
}

RequestResult Controller::execSetProperties(const CommonParams& common, const SetPropertiesParams& params)
{
    Timer timer;
    Error error;
    AggregatedState state{ AggregatedState::Undefined };
    setProperties(common, error, params, state);
    execRequestTrigger("SetProperties", common);
    return createRequestResult(common, error, "SetProperties done", timer.duration(), state);
}

RequestResult Controller::execGetState(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    Error error;
    getState(common, error, params.mPath, state, detailedState.get());
    execRequestTrigger("GetState", common);
    return createRequestResult(common, error, "GetState done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execConfigure(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    Error error;
    changeStateConfigure(common, error, params.mPath, state, detailedState.get());
    execRequestTrigger("Configure", common);
    return createRequestResult(common, error, "Configure done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execStart(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    Error error;
    changeState(common, error, TopoTransition::Run, params.mPath, state, detailedState.get());
    execRequestTrigger("Start", common);
    return createRequestResult(common, error, "Start done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execStop(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    Error error;
    changeState(common, error, TopoTransition::Stop, params.mPath, state, detailedState.get());
    execRequestTrigger("Stop", common);
    return createRequestResult(common, error, "Stop done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execReset(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    Error error;
    changeStateReset(common, error, params.mPath, state, detailedState.get());
    execRequestTrigger("Reset", common);
    return createRequestResult(common, error, "Reset done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execTerminate(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    Error error;
    changeState(common, error, TopoTransition::End, params.mPath, state, detailedState.get());
    execRequestTrigger("Terminate", common);
    return createRequestResult(common, error, "Terminate done", timer.duration(), state, move(detailedState));
}

StatusRequestResult Controller::execStatus(const StatusParams& params)
{
    lock_guard<mutex> lock(mSessionsMtx);
    Timer timer;
    StatusRequestResult result;
    for (const auto& v : mSessions) {
        const auto& info{ v.second };
        PartitionStatus status;
        status.mPartitionID = info->mPartitionID;
        try {
            status.mDDSSessionID = to_string(info->mDDSSession->getSessionID());
            status.mDDSSessionStatus = (info->mDDSSession->IsRunning()) ? DDSSessionStatus::running : DDSSessionStatus::stopped;
        } catch (exception& e) {
            OLOG(warning, status.mPartitionID, 0) << "Failed to get session ID or session status: " << e.what();
        }

        // Filter running sessions if needed
        if ((params.mRunning && status.mDDSSessionStatus == DDSSessionStatus::running) || (!params.mRunning)) {
            try {
                status.mAggregatedState = (info->mTopology != nullptr && info->mDDSTopo != nullptr)
                ?
                aggregateStateForPath(info->mDDSTopo.get(), info->mTopology->GetCurrentState(), "")
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
    result.mExecTime = timer.duration();
    execRequestTrigger("Status", CommonParams());
    return result;
}

void Controller::execRequestTrigger(const string& plugin, const CommonParams& common)
{
    if (mTriggers.isPluginRegistered(plugin)) {
        try {
            OLOG(debug, common) << "Executing request trigger " << quoted(plugin);
            string out{ mTriggers.execPlugin(plugin, "", common.mPartitionID, common.mRunNr) };
            OLOG(debug, common) << "Request trigger " << quoted(plugin) << " done: " << out;
        } catch (exception& e) {
            OLOG(error, common) << "Request trigger " << quoted(plugin) << " failed: " << e.what();
        }
    } else {
        // OLOG(debug, common) << "No plugins registered for " << quoted(plugin);
    }
}

void Controller::updateRestore()
{
    if (mRestoreId.empty()) {
        return;
    }

    OLOG(info) << "Updating restore file " << quoted(mRestoreId) << "...";
    SRestoreData data;

    lock_guard<mutex> lock(mSessionsMtx);
    for (const auto& v : mSessions) {
        const auto& info{ v.second };
        try {
            if (info->mDDSSession->IsRunning()) {
                data.m_partitions.push_back(SRestorePartition(info->mPartitionID, to_string(info->mDDSSession->getSessionID())));
            }
        } catch (exception& e) {
            OLOG(warning, info->mPartitionID, 0) << "Failed to get session ID or session status: " << e.what();
        }
    }

    // Write the the file is locked by mSessionsMtx
    // This is done in order to prevent write failure in case of a parallel execution.
    CRestoreFile(mRestoreId, data).write();
}

RequestResult Controller::createRequestResult(const CommonParams& common, const Error& error, const string& msg, size_t execTime, AggregatedState aggrState, unique_ptr<DetailedState> detailedState/*  = nullptr */)
{
    auto& session = getOrCreateSession(common);
    string sidStr{ to_string(session.mDDSSession->getSessionID()) };
    StatusCode status{ error.mCode ? StatusCode::error : StatusCode::ok };
    return RequestResult(status, msg, execTime, error, common.mPartitionID, common.mRunNr, sidStr, aggrState, move(detailedState));
}

bool Controller::createDDSSession(const CommonParams& common, Error& error)
{
    try {
        auto& session = getOrCreateSession(common);
        boost::uuids::uuid sessionID = session.mDDSSession->create();
        OLOG(info, common) << "DDS session created with session ID: " << to_string(sessionID);
    } catch (exception& e) {
        fillError(common, error, ErrorCode::DDSCreateSessionFailed, toString("Failed to create a DDS session: ", e.what()));
        return false;
    }
    return true;
}

bool Controller::attachToDDSSession(const CommonParams& common, Error& error, const string& sessionID)
{
    try {
        auto& session = getOrCreateSession(common);
        session.mDDSSession->attach(sessionID);
        OLOG(info, common) << "Attached to DDS session: " << sessionID;
    } catch (exception& e) {
        fillError(common, error, ErrorCode::DDSAttachToSessionFailed, toString("Failed to attach to a DDS session: ", e.what()));
        return false;
    }
    return true;
}

bool Controller::submitDDSAgents(Session& session, const CommonParams& common, Error& error, const DDSSubmit::Params& params)
{
    bool success = true;
    using namespace dds::tools_api;

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_submissionTag = common.mPartitionID;
    requestInfo.m_rms = params.mRMSPlugin;
    requestInfo.m_instances = params.mNumAgents;
    requestInfo.m_minInstances = params.mMinAgents;
    requestInfo.m_slots = params.mNumSlots;
    requestInfo.m_config = params.mConfigFile;
    requestInfo.m_envCfgFilePath = params.mEnvFile;
    requestInfo.m_groupName = params.mAgentGroup;
    OLOG(info, common) << "Submitting: " << requestInfo;

    condition_variable cv;

    SSubmitRequest::ptr_t requestPtr = SSubmitRequest::makeRequest(requestInfo);

    requestPtr->setMessageCallback([&success, &error, &common, this](const SMessageResponseData& msg) {
        if (msg.m_severity == dds::intercom_api::EMsgSeverity::error) {
            success = false;
            fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Submit error: ", msg.m_msg));
        } else {
            OLOG(info, common) << "...Submit: " << msg.m_msg;
        }
    });

    requestPtr->setDoneCallback([&cv, &common]() {
        // OLOG(info, common) << "Agent submission done";
        cv.notify_all();
    });

    session.mDDSSession->sendRequest<SSubmitRequest>(requestPtr);

    mutex mtx;
    unique_lock<mutex> lock(mtx);
    cv_status waitStatus = cv.wait_for(lock, requestTimeout(common));

    if (waitStatus == cv_status::timeout) {
        success = false;
        fillError(common, error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
    } else {
        // OLOG(info, common) << "Agent submission done successfully";
    }
    return success;
}

bool Controller::requestCommanderInfo(const CommonParams& common, Error& error, dds::tools_api::SCommanderInfoRequest::response_t& commanderInfo)
{
    using namespace dds::tools_api;
    try {
        stringstream ss;
        auto& session = getOrCreateSession(common);
        session.mDDSSession->syncSendRequest<SCommanderInfoRequest>(SCommanderInfoRequest::request_t(),
                                                                    commanderInfo,
                                                                    requestTimeout(common),
                                                                    &ss);
        OLOG(info, common) << ss.str();
        OLOG(debug, common) << "Commander info: " << commanderInfo;
        return true;
    } catch (exception& e) {
        fillError(common, error, ErrorCode::DDSCommanderInfoFailed, toString("Error getting DDS commander info: ", e.what()));
        return false;
    }
}

bool Controller::waitForNumActiveSlots(Session& session, const CommonParams& common, Error& error, size_t numSlots)
{
    try {
        session.mDDSSession->waitForNumSlots<dds::tools_api::CSession::EAgentState::active>(numSlots, requestTimeout(common));
    } catch (exception& e) {
        fillError(common, error, ErrorCode::RequestTimeout, toString("Timeout waiting for DDS slots: ", e.what()));
        return false;
    }
    return true;
}

bool Controller::activateDDSTopology(const CommonParams& common, Error& error, const string& topologyFile, dds::tools_api::STopologyRequest::request_t::EUpdateType updateType)
{
    bool success = true;

    dds::tools_api::STopologyRequest::request_t topoInfo;
    topoInfo.m_topologyFile = topologyFile;
    topoInfo.m_disableValidation = true;
    topoInfo.m_updateType = updateType;

    condition_variable cv;
    auto& session = getOrCreateSession(common);

    dds::tools_api::STopologyRequest::ptr_t requestPtr = dds::tools_api::STopologyRequest::makeRequest(topoInfo);

    requestPtr->setMessageCallback([&success, &error, &common, this](const dds::tools_api::SMessageResponseData& msg) {
        if (msg.m_severity == dds::intercom_api::EMsgSeverity::error) {
            success = false;
            fillError(common, error, ErrorCode::DDSActivateTopologyFailed, toString("DDS Activate error: ", msg.m_msg));
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

    requestPtr->setResponseCallback([&common, &session](const dds::tools_api::STopologyResponseData& res) {
        OLOG(debug, common) << "DDS Activate Response: " << res;

        // We are not interested in stopped tasks
        if (res.m_activated) {
            TaskDetails task{res.m_agentID, res.m_slotID, res.m_taskID, res.m_collectionID, res.m_path, res.m_host, res.m_wrkDir};
            session.addTaskDetails(move(task));

            if (res.m_collectionID > 0) {
                CollectionDetails collection{res.m_agentID, res.m_slotID, res.m_collectionID, res.m_path, res.m_host, res.m_wrkDir};
                auto pos = collection.mPath.rfind('/');
                if (pos != string::npos) {
                    collection.mPath.erase(pos);
                }
                session.addCollectionDetails(move(collection));
            }
        }
    });

    requestPtr->setDoneCallback([&cv]() { cv.notify_all(); });

    session.mDDSSession->sendRequest<dds::tools_api::STopologyRequest>(requestPtr);

    mutex mtx;
    unique_lock<mutex> lock(mtx);
    cv_status waitStatus = cv.wait_for(lock, requestTimeout(common));

    if (waitStatus == cv_status::timeout) {
        success = false;
        fillError(common, error, ErrorCode::RequestTimeout, "Timed out waiting for topology activation");
        OLOG(error, common) << error;
    }

    // session.debug();

    OLOG(info, common) << "Topology " << quoted(topologyFile) << ((success) ? " activated successfully" : " failed to activate");
    return success;
}

bool Controller::shutdownDDSSession(const CommonParams& common, Error& error)
{
    try {
        auto& session = getOrCreateSession(common);
        session.mTopology.reset();
        session.mDDSTopo.reset();
        session.mNinfo.clear();
        session.mTopoFilePath.clear();
        // We stop the session anyway if session ID is not nil.
        // Session can already be stopped by `dds-session stop` but session ID is not yet reset to nil.
        // If session is already stopped dds::tools_api::CSession::shutdown will reset pointers.
        if (session.mDDSSession->getSessionID() != boost::uuids::nil_uuid()) {
            session.mDDSSession->shutdown();
            if (session.mDDSSession->getSessionID() == boost::uuids::nil_uuid()) {
                OLOG(info, common) << "DDS session has been shut down";
            } else {
                fillError(common, error, ErrorCode::DDSShutdownSessionFailed, "Failed to shut down DDS session");
                return false;
            }
        }
    } catch (exception& e) {
        fillError(common, error, ErrorCode::DDSShutdownSessionFailed, toString("Shutdown failed: ", e.what()));
        return false;
    }
    return true;
}

void Controller::extractNmin(const CommonParams& common, const string& topologyFile)
{
    using namespace dds::topology_api;
    auto& session = getOrCreateSession(common);
    // extract the nmin variables and detect corresponding collections
    session.mNinfo.clear();
    CTopoVars vars;
    vars.initFromXML(topologyFile);
    for (const auto& [var, nmin] : vars.getMap()) {
        if (0 == var.compare(0, 9, "odc_nmin_")) {
            session.mNinfo.emplace(var.substr(9), TopoGroupInfo{0, 0, stoull(nmin), ""});
        }
    }

    CTopology ddsTopo(topologyFile);

    std::map<std::string, std::string> reqs;

    auto requirements = ddsTopo.getMainGroup()->getElementsByType(CTopoBase::EType::REQUIREMENT);
    for (const auto& req : requirements) {
        CTopoRequirement::Ptr_t r = dynamic_pointer_cast<CTopoRequirement>(req);
        reqs.emplace(r->getName(), r->getValue());
    }

    auto groups = ddsTopo.getMainGroup()->getElementsByType(CTopoBase::EType::GROUP);
    for (const auto& group : groups) {
        CTopoGroup::Ptr_t g = dynamic_pointer_cast<CTopoGroup>(group);
        try {
            session.mNinfo.at(g->getName()).nOriginal = g->getN();
            session.mNinfo.at(g->getName()).nCurrent = g->getN();
            auto collections = g->getElementsByType(CTopoBase::EType::COLLECTION);
            if (collections.size() == 1) {
                CTopoCollection::Ptr_t c = dynamic_pointer_cast<CTopoCollection>(collections.at(0));
                auto topoReqs = c->getRequirements();
                for (const auto& tr : topoReqs) {
                    CTopoRequirement::Ptr_t r = dynamic_pointer_cast<CTopoRequirement>(tr);
                    if (r->getRequirementType() == CTopoRequirement::EType::GroupName) {
                        session.mNinfo.at(g->getName()).agentGroup = r->getValue();
                    }
                }
            }
        } catch (out_of_range&) {
            continue;
        }
    }

    OLOG(info, common) << "Groups:";
    for (const auto& [group, nmin] : session.mNinfo) {
        OLOG(info, common) << "  name: " << group
                           << ", n (original): " << nmin.nOriginal
                           << ", n (current): " << nmin.nCurrent
                           << ", n (minimum): " << nmin.nMin
                           << ", agent group: " << nmin.agentGroup;
    }
}

bool Controller::createDDSTopology(const CommonParams& common, Error& error, const string& topologyFile)
{
    using namespace dds::topology_api;
    try {
        auto& session = getOrCreateSession(common);
        session.mDDSTopo = make_unique<CTopology>(topologyFile);
        OLOG(info, common) << "DDS topology " << quoted(topologyFile) << " created successfully";
    } catch (exception& e) {
        fillError(common, error, ErrorCode::DDSCreateTopologyFailed, toString("Failed to initialize DDS topology: ", e.what()));
        return false;
    }
    return true;
}

bool Controller::resetTopology(const CommonParams& common)
{
    auto& session = getOrCreateSession(common);
    session.mTopology.reset();
    return true;
}

bool Controller::createTopology(const CommonParams& common, Error& error, const string& topologyFile)
{
    auto& session = getOrCreateSession(common);
    try {
        session.mTopology.reset();
        session.mTopology = make_unique<Topology>(dds::topology_api::CTopology(topologyFile), session.mDDSSession);
    } catch (exception& e) {
        session.mTopology = nullptr;
        fillError(common, error, ErrorCode::FairMQCreateTopologyFailed, toString("Failed to initialize FairMQ topology: ", e.what()));
    }
    return session.mTopology != nullptr;
}

bool Controller::changeState(const CommonParams& common, Error& error, TopoTransition transition, const string& path, AggregatedState& aggrState, DetailedState* detailedState)
{
    auto& session = getOrCreateSession(common);
    if (session.mTopology == nullptr) {
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    auto it = gExpectedState.find(transition);
    DeviceState expState{ it != gExpectedState.end() ? it->second : DeviceState::Undefined };
    if (expState == DeviceState::Undefined) {
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Unexpected FairMQ transition ", transition));
        return false;
    }

    bool success = true;

    try {
        auto [errorCode, topoState] = session.mTopology->ChangeState(transition, path, requestTimeout(common));

        success = !errorCode;
        if (!success) {
            auto failed = stateSummaryOnFailure(common, session.mTopology->GetCurrentState(), expState, session);
            if (static_cast<ErrorCode>(errorCode.value()) != ErrorCode::DeviceChangeStateInvalidTransition) {
                success = attemptTopoRecovery(failed, session, common);
            } else {
                OLOG(debug, common) << "Invalid transition, skipping nMin check.";
            }
            topoState = session.mTopology->GetCurrentState();
            if (!success) {
                switch (static_cast<ErrorCode>(errorCode.value())) {
                    case ErrorCode::OperationTimeout:
                        fillError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for ", transition, " transition"));
                        break;
                    default:
                        fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("FairMQ change state failed: ", errorCode.message()));
                        break;
                }
            }
        }

        session.fillDetailedState(topoState, detailedState);

        aggrState = AggregateState(topoState);
        if (success) {
            OLOG(info, common) << "State changed to " << aggrState << " via " << transition << " transition";
        }

        printStateStats(common, topoState);

    } catch (exception& e) {
        stateSummaryOnFailure(common, session.mTopology->GetCurrentState(), expState, session);
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", e.what()));
        success = false;
    }

    return success;
}

bool Controller::waitForState(const CommonParams& common, Error& error, DeviceState expState, const string& path)
{
    auto& session = getOrCreateSession(common);
    if (session.mTopology == nullptr) {
        fillError(common, error, ErrorCode::FairMQWaitForStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    OLOG(info, common) << "Waiting for the topology to reach " << expState << " state.";

    bool success = false;

    try {
        error_code errorCode = session.mTopology->WaitForState(expState, path, requestTimeout(common));

        success = !errorCode;
        if (!success) {
            auto failed = stateSummaryOnFailure(common, session.mTopology->GetCurrentState(), expState, session);
            success = attemptTopoRecovery(failed, session, common);
            if (!success) {
                switch (static_cast<ErrorCode>(errorCode.value())) {
                    case ErrorCode::OperationTimeout:
                        fillError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for ", expState, " state"));
                        break;
                    default:
                        fillError(common, error, ErrorCode::FairMQWaitForStateFailed, toString("Failed waiting for ", expState, " state: ", errorCode.message()));
                        break;
                }
            }
        }

        if (success) {
            OLOG(info, common) << "Topology state is now " << expState;
        }
    } catch (exception& e) {
        stateSummaryOnFailure(common, session.mTopology->GetCurrentState(), expState, session);
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Wait for state failed: ", e.what()));
        success = false;
    }

    return success;
}

bool Controller::changeStateConfigure(const CommonParams& common, Error& error, const string& path, AggregatedState& aggrState, DetailedState* detailedState)
{
    return changeState(common, error, TopoTransition::InitDevice,   path, aggrState, detailedState)
        && changeState(common, error, TopoTransition::CompleteInit, path, aggrState, detailedState)
        && changeState(common, error, TopoTransition::Bind,         path, aggrState, detailedState)
        && changeState(common, error, TopoTransition::Connect,      path, aggrState, detailedState)
        && changeState(common, error, TopoTransition::InitTask,     path, aggrState, detailedState);
}

bool Controller::changeStateReset(const CommonParams& common, Error& error, const string& path, AggregatedState& aggrState, DetailedState* detailedState)
{
    return changeState(common, error, TopoTransition::ResetTask,   path, aggrState, detailedState)
        && changeState(common, error, TopoTransition::ResetDevice, path, aggrState, detailedState);
}

bool Controller::getState(const CommonParams& common, Error& error, const string& path, AggregatedState& aggrState, DetailedState* detailedState)
{
    auto& session = getOrCreateSession(common);
    if (session.mTopology == nullptr) {
        error.mCode = MakeErrorCode(ErrorCode::FairMQGetStateFailed);
        error.mDetails = "FairMQ topology is not initialized";
        return false;
    }

    bool success = true;
    auto const topoState = session.mTopology->GetCurrentState();

    try {
        aggrState = aggregateStateForPath(session.mDDSTopo.get(), topoState, path);
    } catch (exception& e) {
        success = false;
        fillError(common, error, ErrorCode::FairMQGetStateFailed, toString("Get state failed: ", e.what()));
    }
    session.fillDetailedState(topoState, detailedState);

    printStateStats(common, topoState);

    return success;
}

bool Controller::setProperties(const CommonParams& common, Error& error, const SetPropertiesParams& params, AggregatedState& aggrState)
{
    auto& session = getOrCreateSession(common);
    if (session.mTopology == nullptr) {
        fillError(common, error, ErrorCode::FairMQSetPropertiesFailed, "FairMQ topology is not initialized");
        return false;
    }

    bool success = true;

    try {
        auto [errorCode, failedDevices] = session.mTopology->SetProperties(params.mProperties, params.mPath, requestTimeout(common));

        success = !errorCode;
        if (success) {
            OLOG(info, common) << "Set property finished successfully";
        } else {
            FailedTasksCollections failed;
            size_t count = 1;
            OLOG(error, common) << "Following devices failed to set properties: ";
            for (auto taskId : failedDevices) {
                try {
                    TaskDetails& taskDetails = session.getTaskDetails(taskId);
                    OLOG(error, common) << "  [" << count << "] " << taskDetails;
                    failed.tasks.push_back(&taskDetails);
                    auto it = find_if(failed.collections.cbegin(), failed.collections.cend(), [&](const auto& ci) {
                        return ci->mCollectionID == taskDetails.mCollectionID;
                    });
                    if (it == failed.collections.cend()) {
                        CollectionDetails& collectionDetails = session.getCollectionDetails(taskDetails.mCollectionID);
                        failed.collections.push_back(&collectionDetails);
                    }
                } catch (const exception& e) {
                    OLOG(error, common) << "Set properties error: " << e.what();
                }
                ++count;
            }
            success = attemptTopoRecovery(failed, session, common);
            if (!success) {
                switch (static_cast<ErrorCode>(errorCode.value())) {
                    case ErrorCode::OperationTimeout:
                        fillError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for set property: ", errorCode.message()));
                        break;
                    default:
                        fillError(common, error, ErrorCode::FairMQSetPropertiesFailed, toString("Set property error message: ", errorCode.message()));
                        break;
                }
            }
        }

        aggrState = AggregateState(session.mTopology->GetCurrentState());
    } catch (exception& e) {
        success = false;
        fillError(common, error, ErrorCode::FairMQSetPropertiesFailed, toString("Set properties failed: ", e.what()));
    }

    return success;
}

AggregatedState Controller::aggregateStateForPath(const dds::topology_api::CTopology* ddsTopo, const TopoState& topoState, const string& path)
{
    if (path.empty())
        return AggregateState(topoState);

    if (ddsTopo == nullptr)
        throw runtime_error("DDS topology is not initialized");

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

        if (taskIds.empty())
            throw runtime_error("No tasks found matching the path " + path);

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

void Controller::fillError(const CommonParams& common, Error& error, ErrorCode errorCode, const string& msg)
{
    error.mCode = MakeErrorCode(errorCode);
    error.mDetails = msg;
    OLOG(error, common) << error;
}

void Controller::fillFatalError(const CommonParams& common, Error& error, ErrorCode errorCode, const string& msg)
{
    error.mCode = MakeErrorCode(errorCode);
    error.mDetails = msg;
    stringstream ss(error.mDetails);
    string line;
    OLOG(fatal, common) << error.mCode;
    while (getline(ss, line, '\n')) {
        OLOG(fatal, common) << line;
    }
}

Controller::Session& Controller::getOrCreateSession(const CommonParams& common)
{
    lock_guard<mutex> lock(mSessionsMtx);
    auto it = mSessions.find(common.mPartitionID);
    if (it == mSessions.end()) {
        auto newSession = make_unique<Session>();
        newSession->mDDSSession = make_unique<dds::tools_api::CSession>();
        newSession->mPartitionID = common.mPartitionID;
        auto ret = mSessions.emplace(common.mPartitionID, move(newSession));
        // OLOG(debug, common) << "Created session for partition ID " << quoted(common.mPartitionID);
        return *(ret.first->second);
    }
    // OLOG(debug, common) << "Found session for partition ID " << quoted(common.mPartitionID);
    return *(it->second);
}

void Controller::removeSession(const CommonParams& common)
{
    lock_guard<mutex> lock(mSessionsMtx);
    auto numRemoved = mSessions.erase(common.mPartitionID);
    if (numRemoved == 1) {
        OLOG(debug, common) << "Removed session for partition ID " << quoted(common.mPartitionID);
    } else {
        OLOG(debug, common) << "Found session for partition ID " << quoted(common.mPartitionID);
    }
}

Error Controller::checkSessionIsRunning(const CommonParams& common, ErrorCode errorCode)
{
    Error error;
    auto& session = getOrCreateSession(common);
    if (!session.mDDSSession->IsRunning()) {
        fillError(common, error, errorCode, "DDS session is not running. Use Init or Run to start the session.");
    }
    return error;
}

FailedTasksCollections Controller::stateSummaryOnFailure(const CommonParams& common, const TopoState& topoState, DeviceState expectedState, Session& session)
{
    FailedTasksCollections failed;

    size_t numTasks = topoState.size();
    size_t numFailedTasks = 0;
    for (const auto& status : topoState) {
        // Print only failed devices
        if (status.state == expectedState) {
            continue;
        }

        ++numFailedTasks;
        if (numFailedTasks == 1) {
            OLOG(error, common) << "Following devices failed to transition to " << expectedState << " state:";
        }
        stringstream ss;
        ss << "  [" << numFailedTasks << "]"
           << " taskID: " << status.taskId
           << ", state: " << status.state
           << ", previous state: " << status.lastState
           << ", collectionID: " << status.collectionId
           << ", subscribed: " << boolalpha << status.subscribedToStateChanges
           << ", ignored: " << boolalpha << status.ignored;

        try {
            TaskDetails& taskDetails = session.getTaskDetails(status.taskId);
            failed.tasks.push_back(&taskDetails);
            ss << ", agentID: " << taskDetails.mAgentID
               << ", slotID: "  << taskDetails.mSlotID
               << ", path: "    << taskDetails.mPath
               << ", host: "    << taskDetails.mHost
               << ", wrkDir: "  << taskDetails.mWrkDir;
        } catch (const exception& e) {
            OLOG(error, common) << "State summary error: " << e.what();
        }
        OLOG(error, common) << ss.str();
    }

    auto collectionMap{ GroupByCollectionId(topoState) };
    size_t numCollections = collectionMap.size();
    size_t numFailedCollections = 0;
    for (const auto& [collectionId, states] : collectionMap) {
        try {
            auto collectionState = AggregateState(states);
            // Print only failed collections
            if (collectionState == expectedState) {
                continue;
            }

            ++numFailedCollections;
            if (numFailedCollections == 1) {
                OLOG(error, common) << "Following collections failed to transition to " << expectedState << " state:";
            }
            stringstream ss;
            ss << "  [" << numFailedCollections << "] state: " << collectionState;

            CollectionDetails& collectionDetails = session.getCollectionDetails(collectionId);
            failed.collections.push_back(&collectionDetails);
            ss << ", " << collectionDetails;
            OLOG(error, common) << ss.str();
        } catch (const exception& e) {
            OLOG(error, common) << "State summary error: " << e.what();
        }
    }

    size_t numSuccessfulTasks = numTasks - numFailedTasks;
    size_t numSuccessfulCollections = numCollections - numFailedCollections;
    OLOG(error, common) << "Summary after transitioning to " << expectedState << " state:";
    OLOG(error, common) << "  [tasks] total: " << numTasks << ", successful: " << numSuccessfulTasks << ", failed: " << numFailedTasks;
    OLOG(error, common) << "  [collections] total: " << numCollections << ", successful: " << numSuccessfulCollections << ", failed: " << numFailedCollections;

    return failed;
}

bool Controller::attemptTopoRecovery(FailedTasksCollections& failed, Session& session, const CommonParams& common)
{
    if (!failed.collections.empty() && !session.mNinfo.empty()) {
        OLOG(info, common) << "Checking if execution can continue according to the minimum number of nodes requirement...";
        // get failed collections and determine if recovery makes sense:
        //   - are failed collections inside of a group?
        //   - do all failed collections have nMin parameter defined?
        //   - is the nMin parameter satisfied?
        map<string, uint64_t> failedCollectionsCount;
        for (const auto& c : failed.collections) {
            // OLOG(info, common) << "Checking collection '" << c->mPath << "' with agend id " << c->mAgentID;
            string collectionParentName = session.mDDSTopo->getRuntimeCollectionById(c->mCollectionID).m_collection->getParent()->getName();
            for (const auto& [groupName, groupInfo] : session.mNinfo) {
                if (collectionParentName != groupName) {
                    // stop recovery, conditions not satisifed
                    OLOG(error, common) << "Failed collection '" << c->mPath << "' is not in a group that has the nmin parameter specified";
                    return false;
                } else {
                    // increment failed collection count per group name
                    if (failedCollectionsCount.find(groupName) == failedCollectionsCount.end()) {
                        failedCollectionsCount.emplace(groupName, 1);
                    } else {
                        failedCollectionsCount[groupName]++;
                    }
                    // OLOG(info, common) << "Group '" << groupName << "', failed collection count " << failedCollectionsCount.at(groupName);
                }
            }
        }

        // proceed only if remaining collections > nmin.
        for (auto& [gName, gInfo] : session.mNinfo) {
            uint64_t failedCount = failedCollectionsCount.at(gName);
            uint64_t remainingCount = gInfo.nCurrent - failedCount;
            OLOG(info, common) << "Group '" << gName << "' with n (original): " << gInfo.nOriginal << ", n (current): " << gInfo.nCurrent << ", nmin: " << gInfo.nMin << ". Failed count: " << failedCount;
            if (remainingCount < gInfo.nMin) {
                OLOG(error, common) << "Number of remaining collections in group '" << gName << "' (" << remainingCount << ") is below nmin (" << gInfo.nMin << ")";
                return false;
            } else {
                gInfo.nCurrent = remainingCount;
            }
        }

        // mark all tasks in the failed collections as failed and set them to be ignored for further actions
        session.mTopology->IgnoreFailedCollections(failed.collections);

        // shutdown agents responsible for failed collections (https://github.com/FairRootGroup/DDS/blob/master/dds-tools-lib/tests/TestSession.cpp#L632)

        using namespace dds::tools_api;
        // using namespace dds::topology_api;

        try {
            size_t currentSlotsCount = session.mTotalSlots;

            size_t numSlotsToRemove = 0;
            for (const auto& c : failed.collections) {
                numSlotsToRemove += session.mAgentSlots.at(c->mAgentID);
            }

            size_t expectedNumSlots = session.mTotalSlots - numSlotsToRemove;
            OLOG(info, common) << "Current number of slots: " << session.mTotalSlots << ", expecting to reduce to " << expectedNumSlots;

            for (const auto& c : failed.collections) {
                OLOG(info, common) << "Sending shutdown signal to agent " << c->mAgentID << ", responsible for " << c->mPath;
                SAgentCommandRequest::request_t agentCmd;
                agentCmd.m_commandType = SAgentCommandRequestData::EAgentCommandType::shutDownByID;
                agentCmd.m_arg1 = c->mAgentID;
                session.mDDSSession->syncSendRequest<SAgentCommandRequest>(agentCmd, requestTimeout(common));
            }

            // TODO: notification on agent shutdown in development in DDS
            OLOG(info, common) << "Current number of slots: " << getNumSlots(session, common);
            currentSlotsCount = getNumSlots(session, common);
            size_t attempts = 0;
            while (currentSlotsCount != expectedNumSlots && attempts < 400) {
                this_thread::sleep_for(chrono::milliseconds(50));
                currentSlotsCount = getNumSlots(session, common);
                // OLOG(info, common) << "Current number of slots: " << currentSlotsCount;
                ++attempts;
            }
            if (currentSlotsCount != expectedNumSlots) {
                OLOG(warning, common) << "Could not reduce the number of slots to " << expectedNumSlots << ", current count is: " << currentSlotsCount;
            } else {
                OLOG(info, common) << "Successfully reduced number of slots to " << currentSlotsCount;
            }
            session.mTotalSlots = currentSlotsCount;
        } catch (exception& e) {
            OLOG(error, common) << "Failed updating nubmer of slots: " << e.what();
            return false;
        }

        return true;
    }

    return false;
}


dds::tools_api::SAgentInfoRequest::responseVector_t Controller::getAgentInfo(Session& session, const CommonParams& common) const
{
    using namespace dds::tools_api;
    SAgentInfoRequest::request_t agentInfoRequest;
    SAgentInfoRequest::responseVector_t agentInfo;
    session.mDDSSession->syncSendRequest<SAgentInfoRequest>(agentInfoRequest, agentInfo, requestTimeout(common));
    return agentInfo;
}

uint32_t Controller::getNumSlots(Session& session, const CommonParams& common) const
{
    using namespace dds::tools_api;
    SAgentCountRequest::response_t agentCountInfo;
    session.mDDSSession->syncSendRequest<SAgentCountRequest>(SAgentCountRequest::request_t(), agentCountInfo, requestTimeout(common));
    return agentCountInfo.m_activeSlotsCount;
}

bool Controller::subscribeToDDSSession(const CommonParams& common, Error& error)
{
    try {
        auto& session = getOrCreateSession(common);
        if (session.mDDSSession->IsRunning()) {
            // Subscrube on TaskDone events
            auto request{ dds::tools_api::SOnTaskDoneRequest::makeRequest(dds::tools_api::SOnTaskDoneRequest::request_t()) };
            request->setResponseCallback([common](const dds::tools_api::SOnTaskDoneResponseData& i) {
                stringstream ss;
                ss << "Task " << i.m_taskID
                   << " with path '" << i.m_taskPath
                   << "' exited with code " << i.m_exitCode
                   << " and signal " << i.m_signal
                   << " on host " << i.m_host
                   << " in working directory '" << i.m_wrkDir << "'";
                if (i.m_exitCode != 0 || i.m_signal != 0) {
                    OLOG(error, common) << ss.str();
                } else {
                    OLOG(debug, common) << ss.str();
                }
            });
            session.mDDSSession->sendRequest<dds::tools_api::SOnTaskDoneRequest>(request);
            OLOG(info, common) << "Subscribed to task done event from session " << quoted(to_string(session.mDDSSession->getSessionID()));
        } else {
            fillError(common, error, ErrorCode::DDSSubscribeToSessionFailed, "Failed to subscribe to task done events: session is not running");
            return false;
        }
    } catch (exception& e) {
        fillError(common, error, ErrorCode::DDSSubscribeToSessionFailed, string("Failed to subscribe to task done events: ") + e.what());
        return false;
    }
    return true;
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
        stringstream ssCmd;
        ssCmd << boost::process::search_path("bash").string() << " -c " << quoted(topologyScript);
        string out;
        string err;
        int exitCode = EXIT_SUCCESS;
        string cmd{ ssCmd.str() };
        OLOG(info, common) << "Executing topology script: " << cmd;
        execute(cmd, requestTimeout(common), &out, &err, &exitCode);

        if (exitCode != EXIT_SUCCESS) {
            throw runtime_error(toString("Topology generation script ", quoted(cmd), " failed with exit code: ", exitCode, "; stderr: ", quoted(err), "; stdout: ", quoted(out)));
        }

        const string shortOut{ out.substr(0, min(out.length(), size_t(20))) };
        OLOG(info, common) << "Topology script executed successfully: stdout (" << quoted(shortOut) << "...) stderr (" << quoted(err) << ")";

        content = out;
    }

    // Create temp topology file with `content`
    const bfs::path tmpPath{ bfs::temp_directory_path() / bfs::unique_path() };
    bfs::create_directories(tmpPath);
    const bfs::path filepath{ tmpPath / "topology.xml" };
    ofstream file(filepath.string());
    if (!file.is_open()) {
        throw runtime_error(toString("Failed to create temp topology file ", quoted(filepath.string())));
    }
    file << content;
    OLOG(info, common) << "Temp topology file " << quoted(filepath.string()) << " created successfully";
    return filepath.string();
}

chrono::seconds Controller::requestTimeout(const CommonParams& common) const
{
    auto timeout{ (common.mTimeout == 0) ? mTimeout : chrono::seconds(common.mTimeout) };
    OLOG(debug, common) << "Request timeout: " << timeout.count() << " sec";
    return timeout;
}

void Controller::registerResourcePlugins(const DDSSubmit::PluginMap_t& pluginMap)
{
    for (const auto& v : pluginMap) {
        mSubmit.registerPlugin(v.first, v.second);
    }
}

void Controller::registerRequestTriggers(const PluginManager::PluginMap_t& triggerMap)
{
    const set<string> avail{ "Initialize", "Submit", "Activate", "Run", "Update", "Configure", "SetProperties", "GetState", "Start", "Stop", "Reset", "Terminate", "Shutdown", "Status" };
    for (const auto& v : triggerMap) {
        if (avail.count(v.first) == 0) {
            throw runtime_error(toString("Failed to add request trigger ", quoted(v.first), ". Invalid request name. Valid names are: ", quoted(boost::algorithm::join(avail, ", "))));
        }
        mTriggers.registerPlugin(v.first, v.second);
    }
}

void Controller::restore(const string& id)
{
    mRestoreId = id;

    OLOG(info) << "Restoring sessions for " << quoted(id);
    auto data{ CRestoreFile(id).read() };
    for (const auto& v : data.m_partitions) {
        OLOG(info, v.mPartitionID, 0) << "Restoring (" << quoted(v.mPartitionID) << "/" << quoted(v.mDDSSessionId) << ")";
        auto result{ execInitialize(CommonParams(v.mPartitionID, 0, 0), InitializeParams(v.mDDSSessionId)) };
        if (result.mError.mCode) {
            OLOG(info, v.mPartitionID, 0) << "Failed to attach to the session. Executing Shutdown trigger for (" << quoted(v.mPartitionID) << "/" << quoted(v.mDDSSessionId) << ")";
            execRequestTrigger("Shutdown", CommonParams(v.mPartitionID, 0, 0));
        } else {
            OLOG(info, v.mPartitionID, 0) << "Successfully attached to the session (" << quoted(v.mPartitionID) << "/" << quoted(v.mDDSSessionId) << ")";
        }
    }
}


void Controller::printStateStats(const CommonParams& common, const TopoState& topoState)
{
    std::map<DeviceState, uint64_t> taskStateCounts;
    std::map<AggregatedState, uint64_t> collectionStateCounts;

    for (const auto& ds : topoState) {
        if (taskStateCounts.find(ds.state) == taskStateCounts.end()) {
            taskStateCounts[ds.state] = 0;
        }
        taskStateCounts[ds.state]++;
    }

    auto collectionMap{ GroupByCollectionId(topoState) };
    for (const auto& [collectionId, states] : collectionMap) {
        AggregatedState collectionState = AggregateState(states);
        if (collectionStateCounts.find(collectionState) == collectionStateCounts.end()) {
            collectionStateCounts[collectionState] = 0;
        }
        collectionStateCounts[collectionState]++;
    }

    stringstream ss;
    ss << "Task states:";
    for (const auto& [state, count] : taskStateCounts) {
        ss << " " << fair::mq::GetStateName(state) << " (" << count << "/" << topoState.size() << ")";
    }
    OLOG(info, common) << ss.str();
    ss.str("");
    ss.clear();
    ss << "Collection states:";
    for (const auto& [state, count] : collectionStateCounts) {
        ss << " " << GetAggregatedStateName(state) << " (" << count << "/" << collectionMap.size() << ")";
    }
    OLOG(info, common) << ss.str();
}