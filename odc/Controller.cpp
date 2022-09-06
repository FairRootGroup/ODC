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
#include <dds/Tools.h>
#include <dds/Topology.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include <algorithm>
#include <filesystem>

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
        bool success = attachToDDSSession(common, error, params.mDDSSessionID)
            && subscribeToDDSSession(common, error);

        // Request current active topology, if any
        // If topology is active, create DDS and FairMQ topology
        if (success) {
            dds::tools_api::SCommanderInfoRequest::response_t commanderInfo;
            requestCommanderInfo(common, error, commanderInfo)
                && !commanderInfo.m_activeTopologyPath.empty()
                && createDDSTopology(common, error, commanderInfo.m_activeTopologyPath)
                && createTopology(common, error);
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

    submit(common, session, error, params.mPlugin, params.mResources);

    execRequestTrigger("Submit", common);
    return createRequestResult(common, error, "Submit done", timer.duration(), AggregatedState::Undefined);
}

void Controller::submit(const CommonParams& common, Session& session, Error& error, const string& plugin, const string& res)
{
    if (!session.mDDSSession->IsRunning()) {
        fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, "DDS session is not running. Use Init or Run to start the session.");
        return;
    }

    // Get DDS submit parameters from ODC resource plugin
    vector<DDSSubmit::Params> ddsParams;
    if (!error.mCode) {
        try {
            ddsParams = mSubmit.makeParams(plugin, res, common.mPartitionID, common.mRunNr, session.mZoneInfos);
        } catch (exception& e) {
            fillError(common, error, ErrorCode::ResourcePluginFailed, toString("Resource plugin failed: ", e.what()));
        }
    }

    if (!error.mCode) {
        OLOG(info, common) << "Preparing to submit " << ddsParams.size() << " configurations:";
        for (unsigned int i = 0; i < ddsParams.size(); ++i) {
            OLOG(info, common) << "  [" << i + 1 << "/" << ddsParams.size() << "]: " << ddsParams.at(i);
        }

        for (unsigned int i = 0; i < ddsParams.size(); ++i) {
            auto it = find_if(session.mNinfo.cbegin(), session.mNinfo.cend(), [&](const auto& tgi) {
                return tgi.second.agentGroup == ddsParams.at(i).mAgentGroup;
            });
            if (it != session.mNinfo.cend()) {
                ddsParams.at(i).mMinAgents = it->second.nMin;
            }
            OLOG(info, common) << "Submitting [" << i + 1 << "/" << ddsParams.size() << "]: " << ddsParams.at(i);

            if (submitDDSAgents(common, session, error, ddsParams.at(i))) {
                session.mTotalSlots += ddsParams.at(i).mNumAgents * ddsParams.at(i).mNumSlots;
            } else {
                OLOG(error, common) << "Submission failed";
                break;
            }
        }
        if (!error.mCode) {
            OLOG(info, common) << "Waiting for " << session.mTotalSlots << " slots...";
            if (waitForNumActiveSlots(common, session, error, session.mTotalSlots)) {
                OLOG(info, common) << "Done waiting for " << session.mTotalSlots << " slots.";
            }
        }
    }

    if (session.mDDSSession->IsRunning()) {
        map<string, uint32_t> agentCounts; // agent count sorted by their group name

        for (const auto& p : ddsParams) {
            agentCounts[p.mAgentGroup] = 0;
        }

        try {
            auto agentInfo = getAgentInfo(common, session);
            OLOG(info, common) << "Launched " << agentInfo.size() << " DDS agents:";
            for (const auto& ai : agentInfo) {
                agentCounts[ai.m_groupName]++;
                session.mAgentSlots[ai.m_agentID] = ai.m_nSlots;
                OLOG(info, common) << "  Agent ID: " << ai.m_agentID
                                // << ", pid: " << ai.m_agentPid
                                << ", host: " << ai.m_host
                                << ", path: " << ai.m_DDSPath
                                << ", group: " << ai.m_groupName
                                // << ", index: " << ai.m_index
                                // << ", username: " << ai.m_username
                                << ", slots: " << ai.m_nSlots << " (idle: " << ai.m_nIdleSlots << ", executing: " << ai.m_nExecutingSlots << ").";
            }
            OLOG(info, common) << "Launched " << agentCounts.size() << " DDS agent groups:";
            for (const auto& [groupName, count] : agentCounts) {
                OLOG(info, common) << "  " << groupName << ": " << count << " agents";
            }
        } catch (const exception& e) {
            fillError(common, error, ErrorCode::DDSCommanderInfoFailed, toString("Failed getting agent info: ", e.what()));
        }

        if (error.mCode) {
            attemptSubmitRecovery(common, session, error, ddsParams, agentCounts);
        }
    }
}

void Controller::attemptSubmitRecovery(const CommonParams& common,
                                       Session& session,
                                       Error& error,
                                       const vector<DDSSubmit::Params>& ddsParams,
                                       const map<string, uint32_t>& agentCounts)
{
    error = Error();
    try {
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
            try {
                session.mTotalSlots = getNumSlots(common, session);
                for (auto& tgi : session.mNinfo) {
                    auto it = find_if(agentCounts.cbegin(), agentCounts.cend(), [&](const auto& ac) {
                        return ac.first == tgi.second.agentGroup;
                    });
                    if (it != agentCounts.cend()) {
                        tgi.second.nCurrent = it->second;
                    }
                }
                updateTopology(common, session);
            } catch (const exception& e) {
                fillError(common, error, ErrorCode::DDSCreateTopologyFailed, toString("Failed updating topology: ", e.what()));
            }
        }
    } catch (const exception& e) {
        fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Recovery failed: ", e.what()));
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
    Timer timer;
    Error error;

    auto& session = getOrCreateSession(common);

    if (!session.mDDSSession->IsRunning()) {
        fillError(common, error, ErrorCode::DDSActivateTopologyFailed, "DDS session is not running. Use Init or Run to start the session.");
    }

    try {
        session.mTopoFilePath = topoFilepath(common, params.mTopoFile, params.mTopoContent, params.mTopoScript);
        extractRequirements(common, session);
    } catch (exception& e) {
        fillFatalErrorLineByLine(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
    }

    if (!error.mCode) {
        activate(common, session, error);
    }

    AggregatedState state{ !error.mCode ? AggregatedState::Idle : AggregatedState::Undefined };
    execRequestTrigger("Activate", common);
    return createRequestResult(common, error, "Activate done", timer.duration(), state);
}

void Controller::activate(const CommonParams& common, Session& session, Error& error)
{
    activateDDSTopology(common, error, session.mTopoFilePath, dds::tools_api::STopologyRequest::request_t::EUpdateType::ACTIVATE)
        && createDDSTopology(common, error, session.mTopoFilePath)
        && createTopology(common, error)
        && waitForState(common, error, DeviceState::Idle, "");
}

RequestResult Controller::execRun(const CommonParams& common, const RunParams& params)
{
    Timer timer;
    Error error;

    auto& session = getOrCreateSession(common);

    if (!session.mRunAttempted) {
        session.mRunAttempted = true;

        // Create new DDS session
        // Shutdown DDS session if it is running already
        shutdownDDSSession(common, error)
            && createDDSSession(common, error)
            && subscribeToDDSSession(common, error);

        execRequestTrigger("Initialize", common);
        updateRestore();

        if (!error.mCode) {
            try {
                session.mTopoFilePath = topoFilepath(common, params.mTopoFile, params.mTopoContent, params.mTopoScript);
                extractRequirements(common, session);
            } catch (exception& e) {
                fillFatalErrorLineByLine(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
            }

            if (!error.mCode) {
                if (!session.mDDSSession->IsRunning()) {
                    fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, "DDS session is not running. Use Init or Run to start the session.");
                }

                submit(common, session, error, params.mPlugin, params.mResources);

                if (!session.mDDSSession->IsRunning()) {
                    fillError(common, error, ErrorCode::DDSActivateTopologyFailed, "DDS session is not running. Use Init or Run to start the session.");
                }

                if (!error.mCode) {
                    activate(common, session, error);
                }
            }
        }
    } else {
        error = Error(MakeErrorCode(ErrorCode::RequestNotSupported), "Repeated Run request is not supported. Shutdown this partition to retry.");
    }

    AggregatedState state{ !error.mCode ? AggregatedState::Idle : AggregatedState::Undefined };
    execRequestTrigger("Run", common);
    return createRequestResult(common, error, "Run done", timer.duration(), state);
}

RequestResult Controller::execUpdate(const CommonParams& common, const UpdateParams& params)
{
    Timer timer;
    Error error;

    auto& session = getOrCreateSession(common);

    AggregatedState state = AggregatedState::Undefined;

    try {
        session.mTopoFilePath = topoFilepath(common, params.mTopoFile, params.mTopoContent, params.mTopoScript);
        extractRequirements(common, session);
    } catch (exception& e) {
        fillFatalErrorLineByLine(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", e.what()));
    }

    if (!error.mCode) {
        changeStateReset(common, error, "", state)
            && resetTopology(session)
            && activateDDSTopology(common, error, session.mTopoFilePath, dds::tools_api::STopologyRequest::request_t::EUpdateType::UPDATE)
            && createDDSTopology(common, error, session.mTopoFilePath)
            && createTopology(common, error)
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

    // grab session id before shutting down the session, to return it in the reply
    string ddsSessionId;
    {
        auto& session = getOrCreateSession(common);
        ddsSessionId = to_string(session.mDDSSession->getSessionID());
    }

    shutdownDDSSession(common, error);
    removeSession(common);
    updateRestore();
    execRequestTrigger("Shutdown", common);

    StatusCode status = error.mCode ? StatusCode::error : StatusCode::ok;
    return RequestResult(status, "Shutdown done", timer.duration(), error, common.mPartitionID, common.mRunNr, ddsSessionId, AggregatedState::Undefined, nullptr);
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
    Error error;

    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    getState(common, error, params.mPath, state, detailedState.get());
    execRequestTrigger("GetState", common);
    return createRequestResult(common, error, "GetState done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execConfigure(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    Error error;

    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    changeStateConfigure(common, error, params.mPath, state, detailedState.get());
    execRequestTrigger("Configure", common);
    return createRequestResult(common, error, "Configure done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execStart(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    Error error;

    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    changeState(common, error, TopoTransition::Run, params.mPath, state, detailedState.get());
    execRequestTrigger("Start", common);
    return createRequestResult(common, error, "Start done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execStop(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    Error error;

    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    changeState(common, error, TopoTransition::Stop, params.mPath, state, detailedState.get());
    execRequestTrigger("Stop", common);
    return createRequestResult(common, error, "Stop done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execReset(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    Error error;

    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
    changeStateReset(common, error, params.mPath, state, detailedState.get());
    execRequestTrigger("Reset", common);
    return createRequestResult(common, error, "Reset done", timer.duration(), state, move(detailedState));
}

RequestResult Controller::execTerminate(const CommonParams& common, const DeviceParams& params)
{
    Timer timer;
    Error error;

    AggregatedState state{ AggregatedState::Undefined };
    unique_ptr<DetailedState> detailedState = params.mDetailed ? make_unique<DetailedState>() : nullptr;
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

    RestoreData data;

    lock_guard<mutex> lock(mSessionsMtx);
    for (const auto& v : mSessions) {
        const auto& info{ v.second };
        try {
            if (info->mDDSSession->IsRunning()) {
                data.mPartitions.push_back(RestorePartition(info->mPartitionID, to_string(info->mDDSSession->getSessionID())));
            }
        } catch (exception& e) {
            OLOG(warning, info->mPartitionID, 0) << "Failed to get session ID or session status: " << e.what();
        }
    }

    // Writing the file is locked by mSessionsMtx
    // This is done in order to prevent write failure in case of a parallel execution.
    RestoreFile(mRestoreId, mRestoreDir, data).write();
}

void Controller::updateHistory(const CommonParams& common, const std::string& sessionId)
{
    if (mHistoryDir.empty()) {
        return;
    }

    lock_guard<mutex> lock(mSessionsMtx);

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
        updateHistory(common, to_string(sessionID));
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

bool Controller::submitDDSAgents(const CommonParams& common, Session& session, Error& error, const DDSSubmit::Params& params)
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

    // DDS does not support ncores parameter directly, set it here through additional config in case of Slurm
    if (params.mRMSPlugin == "slurm" && params.mNumCores > 0) {
        // the following disables `#SBATCH --cpus-per-task=%DDS_NSLOTS%` of DDS for Slurm
        requestInfo.setFlag(SSubmitRequestData::ESubmitRequestFlags::enable_overbooking, true);

        requestInfo.m_inlineConfig = string("#SBATCH --cpus-per-task=" + to_string(params.mNumCores));
    }

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

bool Controller::waitForNumActiveSlots(const CommonParams& common, Session& session, Error& error, size_t numSlots)
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
        session.mZoneInfos.clear();
        session.mTopoFilePath.clear();

        if (session.mDDSSession->getSessionID() != boost::uuids::nil_uuid()) {
            session.mDDSSession->shutdown();
            if (session.mDDSSession->getSessionID() == boost::uuids::nil_uuid()) {
                OLOG(info, common) << "DDS session has been shut down";
            } else {
                fillError(common, error, ErrorCode::DDSShutdownSessionFailed, "Failed to shut down DDS session");
                return false;
            }
        } else {
            OLOG(info, common) << "The session ID for the current DDS session is already zero. Not calling shutdown().";
        }
    } catch (exception& e) {
        fillError(common, error, ErrorCode::DDSShutdownSessionFailed, toString("Shutdown failed: ", e.what()));
        return false;
    }
    return true;
}

void Controller::extractRequirements(const CommonParams& common, Session& session)
{
    using namespace dds::topology_api;

    CTopology ddsTopo(session.mTopoFilePath);

    auto collections = ddsTopo.getMainGroup()->getElementsByType(CTopoBase::EType::COLLECTION);
    OLOG(info, common) << "Extracting requirements:";
    for (const auto& collection : collections) {
        CTopoCollection::Ptr_t c = dynamic_pointer_cast<CTopoCollection>(collection);
        auto colReqs = c->getRequirements();

        string agentGroup;
        string zone;
        int ncores = 0;
        int32_t n = c->getTotalCounter();
        int32_t nmin = -1;

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
                    ncores = stoi(cr->getValue());
                } else if (strStartsWith(cr->getName(), "odc_zone_")) {
                    zone = cr->getValue();
                } else if (strStartsWith(cr->getName(), "odc_nmin_")) {
                    // nMin is only relevant for collections that are in a group (which is not the main group)
                    auto parent = c->getParent();
                    if (parent->getType() == CTopoBase::EType::GROUP && parent->getName() != "main") {
                        nmin = stoull(cr->getValue());
                    } else {
                        // OLOG(info, common) << "collection " << c->getName() << " is not in a group, skipping nMin requirement";
                    }
                }
            } else {
                ss << ", type: Unknown";
            }
            OLOG(info, common) << ss.str();
        }

        if (!agentGroup.empty() && nmin >= 0) {
            auto it = session.mNinfo.find(c->getName());
            if (it == session.mNinfo.end()) {
                session.mNinfo.try_emplace(c->getName(), CollectionNInfo{ n, n, nmin, agentGroup });
            } else {
                // OLOG(info, common) << "collection " << c->getName() << " is already in the mNinfo";
            }
        }

        if (!agentGroup.empty() && !zone.empty()) {
            auto it = session.mZoneInfos.find(zone);
            if (it == session.mZoneInfos.end()) {
                session.mZoneInfos.try_emplace(zone, std::vector<ZoneGroup>{ ZoneGroup{n, ncores, agentGroup} });
            } else {
                it->second.emplace_back(ZoneGroup{n, ncores, agentGroup});
            }
        }
    }

    if (!session.mZoneInfos.empty()) {
        OLOG(info, common) << "Zones from the topology:";
        for (const auto& z : session.mZoneInfos) {
            OLOG(info, common) << "  " << quoted(z.first) << ":";
            for (const auto& zi : z.second) {
                OLOG(info, common) << "    n: " << zi.n << ", ncores: " << zi.ncores << ", agentGroup: " << zi.agentGroup;
            }
        }
    }

    OLOG(info, common) << "N info:";
    for (const auto& [collection, nmin] : session.mNinfo) {
        OLOG(info, common) << "  name: " << collection
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

bool Controller::resetTopology(Session& session)
{
    session.mTopology.reset();
    return true;
}

bool Controller::createTopology(const CommonParams& common, Error& error)
{
    auto& session = getOrCreateSession(common);
    try {
        session.mTopology = make_unique<Topology>(*(session.mDDSTopo), session.mDDSSession);
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
            auto failed = stateSummaryOnFailure(common, session, session.mTopology->GetCurrentState(), expState);
            if (static_cast<ErrorCode>(errorCode.value()) != ErrorCode::DeviceChangeStateInvalidTransition) {
                success = attemptTopoRecovery(common, session, failed);
            } else {
                OLOG(debug, common) << "Invalid transition, skipping nMin check.";
            }
            topoState = session.mTopology->GetCurrentState();
            if (!success) {
                switch (static_cast<ErrorCode>(errorCode.value())) {
                    case ErrorCode::OperationTimeout:
                        fillFatalError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for ", transition, " transition"));
                        break;
                    default:
                        fillFatalError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", errorCode.message()));
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
        stateSummaryOnFailure(common, session, session.mTopology->GetCurrentState(), expState);
        fillFatalError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", e.what()));
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
            auto failed = stateSummaryOnFailure(common, session, session.mTopology->GetCurrentState(), expState);
            success = attemptTopoRecovery(common, session, failed);
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
        stateSummaryOnFailure(common, session, session.mTopology->GetCurrentState(), expState);
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
            success = attemptTopoRecovery(common, session, failed);
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

void Controller::fillError(const CommonParams& common, Error& error, ErrorCode errorCode, const string& msg)
{
    error.mCode = MakeErrorCode(errorCode);
    error.mDetails = msg;
    OLOG(error, common) << error.mDetails;
}

void Controller::fillFatalError(const CommonParams& common, Error& error, ErrorCode errorCode, const string& msg)
{
    error.mCode = MakeErrorCode(errorCode);
    error.mDetails = msg;
    OLOG(fatal, common) << error.mDetails;
}

void Controller::fillFatalErrorLineByLine(const CommonParams& common, Error& error, ErrorCode errorCode, const string& msg)
{
    error.mCode = MakeErrorCode(errorCode);
    error.mDetails = msg;
    stringstream ss(error.mDetails);
    string line;
    // OLOG(fatal, common) << error.mCode;
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
    size_t numRemoved = mSessions.erase(common.mPartitionID);
    if (numRemoved == 1) {
        OLOG(debug, common) << "Removed session for partition ID " << quoted(common.mPartitionID);
    } else {
        OLOG(debug, common) << "Found no session for partition ID " << quoted(common.mPartitionID);
    }
}

FailedTasksCollections Controller::stateSummaryOnFailure(const CommonParams& common, Session& session, const TopoState& topoState, DeviceState expectedState)
{
    FailedTasksCollections failed;

    size_t numTasks = topoState.size();
    size_t numFailedTasks = 0;
    for (const auto& status : topoState) {
        if (status.state == expectedState || status.ignored) {
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
            if (collectionState == expectedState) {
                continue;
            }

            if (std::all_of(states.cbegin(), states.cend(), [](const auto& status){ return status.ignored; })) {
                // std::cout << "All devices in " << collectionId << " are ignored" << std::endl;
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

bool Controller::attemptTopoRecovery(const CommonParams& common, Session& session, FailedTasksCollections& failed)
{
    if (!failed.collections.empty() && !session.mNinfo.empty()) {
        OLOG(info, common) << "Checking if execution can continue according to the minimum number of nodes requirement...";
        // get failed collections and determine if recovery makes sense:
        //   - are failed collections inside of a group?
        //   - do all failed collections have nMin parameter defined?
        //   - is the nMin parameter satisfied?
        map<string, int32_t> failedCollectionsCount;
        for (const auto& c : failed.collections) {
            string collectionName = session.mDDSTopo->getRuntimeCollectionById(c->mCollectionID).m_collection->getName();
            OLOG(info, common) << "Checking collection '" << c->mPath << "' with agend id " << c->mAgentID << ", name in the topology: " << collectionName;
            string collectionParentName = session.mDDSTopo->getRuntimeCollectionById(c->mCollectionID).m_collection->getParent()->getName();
            auto it = session.mNinfo.find(collectionName);
            if (it != session.mNinfo.end()) {
                if (failedCollectionsCount.find(collectionName) == failedCollectionsCount.end()) {
                    failedCollectionsCount.emplace(collectionName, 1);
                } else {
                    failedCollectionsCount[collectionName]++;
                }
            } else {
                OLOG(error, common) << "Failed collection '" << c->mPath << "' is not in a group that has the nmin parameter specified";
                return false;
            }
        }

        // proceed only if remaining collections > nmin.
        for (auto& [colName, nInfo] : session.mNinfo) {
            int32_t failedCount = failedCollectionsCount.at(colName);
            int32_t remainingCount = nInfo.nCurrent - failedCount;
            OLOG(info, common) << "Collection '" << colName << "' with n (original): " << nInfo.nOriginal << ", n (current): " << nInfo.nCurrent << ", nmin: " << nInfo.nMin << ". Failed count: " << failedCount;
            if (remainingCount < nInfo.nMin) {
                OLOG(error, common) << "Number of remaining '" << colName << "' collections (" << remainingCount << ") is below nmin (" << nInfo.nMin << ")";
                return false;
            } else {
                nInfo.nCurrent = remainingCount;
            }
        }

        // mark all tasks in the failed collections as failed and set them to be ignored for further actions
        session.mTopology->IgnoreFailedCollections(failed.collections);

        // shutdown agents responsible for failed collections

        using namespace dds::tools_api;

        try {
            size_t currentSlotCount = session.mTotalSlots;

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
            currentSlotCount = getNumSlots(common, session);
            OLOG(info, common) << "Current number of slots: " << currentSlotCount;
            size_t attempts = 0;
            while (currentSlotCount != expectedNumSlots && attempts < 400) {
                this_thread::sleep_for(chrono::milliseconds(50));
                currentSlotCount = getNumSlots(common, session);
                // OLOG(info, common) << "Current number of slots: " << currentSlotCount;
                ++attempts;
            }
            if (currentSlotCount != expectedNumSlots) {
                OLOG(warning, common) << "Could not reduce the number of slots to " << expectedNumSlots << ", current count is: " << currentSlotCount;
            } else {
                OLOG(info, common) << "Successfully reduced number of slots to " << currentSlotCount;
            }
            session.mTotalSlots = currentSlotCount;
        } catch (exception& e) {
            OLOG(error, common) << "Failed updating nubmer of slots: " << e.what();
            return false;
        }

        return true;
    }

    return false;
}

dds::tools_api::SAgentInfoRequest::responseVector_t Controller::getAgentInfo(const CommonParams& common, Session& session) const
{
    using namespace dds::tools_api;
    SAgentInfoRequest::responseVector_t agentInfo;
    session.mDDSSession->syncSendRequest<SAgentInfoRequest>(SAgentInfoRequest::request_t(), agentInfo, requestTimeout(common));
    return agentInfo;
}

uint32_t Controller::getNumSlots(const CommonParams& common, Session& session) const
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

void Controller::registerResourcePlugins(const DDSSubmit::PluginMap& pluginMap)
{
    for (const auto& v : pluginMap) {
        mSubmit.registerPlugin(v.first, v.second);
    }
}

void Controller::registerRequestTriggers(const PluginManager::PluginMap& triggerMap)
{
    const set<string> avail{ "Initialize", "Submit", "Activate", "Run", "Update", "Configure", "SetProperties", "GetState", "Start", "Stop", "Reset", "Terminate", "Shutdown", "Status" };
    for (const auto& v : triggerMap) {
        if (avail.count(v.first) == 0) {
            throw runtime_error(toString("Failed to add request trigger ", quoted(v.first), ". Invalid request name. Valid names are: ", quoted(boost::algorithm::join(avail, ", "))));
        }
        mTriggers.registerPlugin(v.first, v.second);
    }
}

void Controller::restore(const string& id, const string& dir)
{
    mRestoreId = id;
    mRestoreDir = dir;

    OLOG(info) << "Restoring sessions for " << quoted(id);
    auto data{ RestoreFile(id, dir).read() };
    for (const auto& v : data.mPartitions) {
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