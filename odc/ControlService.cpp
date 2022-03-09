/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ODC
#include <odc/ControlService.h>
#include <odc/DDSSubmit.h>
#include <odc/Error.h>
#include <odc/Logger.h>
#include <odc/Process.h>
#include <odc/Restore.h>
#include <odc/Stats.h>
#include <odc/TimeMeasure.h>
#include <odc/Topology.h>
// DDS
#include <dds/Tools.h>
#include <dds/Topology.h>
// BOOST
#include <boost/algorithm/string.hpp>
// STD
#include <algorithm>

using namespace odc;
using namespace odc::core;
using namespace std;
using namespace dds;
using namespace dds::tools_api;
using namespace dds::topology_api;

void ControlService::execRequestTrigger(const string& _plugin, const SCommonParams& common)
{
    if (m_triggers.isPluginRegistered(_plugin)) {
        try {
            OLOG(debug, common) << "Executing request trigger " << quoted(_plugin);
            string out{ m_triggers.execPlugin(_plugin, "", common.m_partitionID, common.m_runNr) };
            OLOG(debug, common) << "Request trigger " << quoted(_plugin) << " done: " << out;
        } catch (exception& _e) {
            OLOG(error, common) << "Request trigger " << quoted(_plugin) << " failed: " << _e.what();
        }
    } else {
        OLOG(debug, common) << "No plugins registered for " << quoted(_plugin);
    }
}

void ControlService::updateRestore()
{
    if (m_restoreId.empty()) {
        return;
    }

    OLOG(info) << "Updating restore file " << quoted(m_restoreId) << "...";
    SRestoreData data;

    lock_guard<mutex> lock(mSessionsMtx);
    for (const auto& v : m_sessions) {
        const auto& info{ v.second };
        try {
            if (info->m_session->IsRunning()) {
                data.m_partitions.push_back(SRestorePartition(info->m_partitionID, to_string(info->m_session->getSessionID())));
            }
        } catch (exception& _e) {
            OLOG(warning, info->m_partitionID, 0) << "Failed to get session ID or session status: " << _e.what();
        }
    }

    // Write the the file is locked by mSessionsMtx
    // This is done in order to prevent write failure in case of a parallel execution.
    CRestoreFile(m_restoreId, data).write();
}

RequestResult ControlService::createRequestResult(const SCommonParams& common,
                                                const SError& error,
                                                const std::string& msg,
                                                size_t execTime,
                                                AggregatedTopologyState aggregatedState,
                                                std::unique_ptr<TopologyState> fullState/*  = nullptr */)
{
    auto& info = getOrCreateSessionInfo(common);
    string sidStr{ to_string(info.m_session->getSessionID()) };
    EStatusCode status{ error.m_code ? EStatusCode::error : EStatusCode::ok };
    return RequestResult(status, msg, execTime, error, common.m_partitionID, common.m_runNr, sidStr, aggregatedState, std::move(fullState));
}

bool ControlService::createDDSSession(const SCommonParams& common, SError& error)
{
    try {
        auto& info = getOrCreateSessionInfo(common);
        boost::uuids::uuid sessionID{ info.m_session->create() };
        OLOG(info, common) << "DDS session created with session ID: " << to_string(sessionID);
    } catch (exception& _e) {
        fillError(common, error, ErrorCode::DDSCreateSessionFailed, toString("Failed to create a DDS session: ", _e.what()));
        return false;
    }
    return true;
}

bool ControlService::attachToDDSSession(const SCommonParams& common, SError& error, const std::string& _sessionID)
{
    try {
        auto& info = getOrCreateSessionInfo(common);
        info.m_session->attach(_sessionID);
        OLOG(info, common) << "Attach to a DDS session with session ID: " << _sessionID;
    } catch (exception& _e) {
        fillError(common, error, ErrorCode::DDSAttachToSessionFailed, toString("Failed to attach to a DDS session: ", _e.what()));
        return false;
    }
    return true;
}

bool ControlService::submitDDSAgents(const SCommonParams& common, SError& error, const CDDSSubmit::SParams& _params)
{
    bool success(true);

    SSubmitRequest::request_t requestInfo;
    requestInfo.m_rms = _params.m_rmsPlugin;
    requestInfo.m_instances = _params.m_numAgents;
    requestInfo.m_slots = _params.m_numSlots;
    requestInfo.m_config = _params.m_configFile;
    requestInfo.m_groupName = _params.m_agentGroup;
    OLOG(info) << "dds::tools_api::SSubmitRequest: " << requestInfo;

    std::condition_variable cv;

    SSubmitRequest::ptr_t requestPtr = SSubmitRequest::makeRequest(requestInfo);

    requestPtr->setMessageCallback([&success, &error, &common, this](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error) {
            success = false;
            fillError(common, error, ErrorCode::DDSSubmitAgentsFailed, toString("Submit error: ", _message.m_msg));
        } else {
            OLOG(info, common) << "Submit: " << _message.m_msg;
        }
    });

    requestPtr->setDoneCallback([&cv, &common]() {
        OLOG(info, common) << "Agent submission done";
        cv.notify_all();
    });

    auto& info = getOrCreateSessionInfo(common);
    info.m_session->sendRequest<SSubmitRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, requestTimeout(common));

    if (waitStatus == std::cv_status::timeout) {
        success = false;
        fillError(common, error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
    } else {
        OLOG(info, common) << "Agent submission done successfully";
    }
    return success;
}

bool ControlService::requestCommanderInfo(const SCommonParams& common, SError& error, SCommanderInfoRequest::response_t& _commanderInfo)
{
    try {
        stringstream ss;
        auto& info = getOrCreateSessionInfo(common);
        info.m_session->syncSendRequest<SCommanderInfoRequest>(SCommanderInfoRequest::request_t(), _commanderInfo, requestTimeout(common), &ss);
        OLOG(info, common) << ss.str();
        OLOG(debug, common) << "Commander info: " << _commanderInfo;
        return true;
    } catch (exception& _e) {
        fillError(common, error, ErrorCode::DDSCommanderInfoFailed, toString("Error getting DDS commander info: ", _e.what()));
        return false;
    }
}

bool ControlService::waitForNumActiveAgents(const SCommonParams& common, SError& error, size_t _numAgents)
{
    try {
        auto& info = getOrCreateSessionInfo(common);
        info.m_session->waitForNumAgents<CSession::EAgentState::active>(_numAgents, requestTimeout(common));
    } catch (std::exception& _e) {
        fillError(common, error, ErrorCode::RequestTimeout, toString("Timeout waiting for DDS agents: ", _e.what()));
        return false;
    }
    return true;
}

bool ControlService::activateDDSTopology(const SCommonParams& common, SError& error, const string& _topologyFile, STopologyRequest::request_t::EUpdateType _updateType)
{
    bool success(true);

    STopologyRequest::request_t topoInfo;
    topoInfo.m_topologyFile = _topologyFile;
    topoInfo.m_disableValidation = true;
    topoInfo.m_updateType = _updateType;

    std::condition_variable cv;
    auto& sessionInfo = getOrCreateSessionInfo(common);

    STopologyRequest::ptr_t requestPtr{ STopologyRequest::makeRequest(topoInfo) };

    requestPtr->setMessageCallback([&success, &error, &common, this](const SMessageResponseData& msg) {
        if (msg.m_severity == dds::intercom_api::EMsgSeverity::error) {
            success = false;
            fillError(common, error, ErrorCode::DDSActivateTopologyFailed, toString("Activate error: ", msg.m_msg));
        } else {
            OLOG(debug, common) << "Activate: " << msg.m_msg;
        }
    });

    requestPtr->setProgressCallback([&common](const SProgressResponseData& progress) {
        uint32_t completed{ progress.m_completed + progress.m_errors };
        if (completed == progress.m_total) {
            OLOG(info, common) << "Activated tasks (" << progress.m_completed << "), errors (" << progress.m_errors << "), total (" << progress.m_total << ")";
        }
    });

    requestPtr->setResponseCallback([&common, &sessionInfo](const STopologyResponseData& res) {
        OLOG(debug, common) << "Activate: " << res;

        // We are not interested in stopped tasks
        if (res.m_activated) {
            TopoTaskInfo task{res.m_agentID, res.m_slotID, res.m_taskID, res.m_path, res.m_host, res.m_wrkDir};
            sessionInfo.addToTaskCache(std::move(task));

            if (res.m_collectionID > 0) {
                TopoCollectionInfo collection{res.m_agentID, res.m_slotID, res.m_collectionID, res.m_path, res.m_host, res.m_wrkDir};
                auto pos = collection.mPath.rfind('/');
                if (pos != std::string::npos) {
                    collection.mPath.erase(pos);
                }
                sessionInfo.addToCollectionCache(std::move(collection));
            }
        }
    });

    requestPtr->setDoneCallback([&cv]() { cv.notify_all(); });

    sessionInfo.m_session->sendRequest<STopologyRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus{ cv.wait_for(lock, requestTimeout(common)) };

    if (waitStatus == std::cv_status::timeout) {
        success = false;
        fillError(common, error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
        OLOG(error, common) << error;
    }

    sessionInfo.debug();

    OLOG(info, common) << "Topology " << quoted(_topologyFile) << ((success) ? " activated successfully" : " failed to activate");
    return success;
}

bool ControlService::shutdownDDSSession(const SCommonParams& common, SError& error)
{
    try {
        auto& info = getOrCreateSessionInfo(common);
        info.m_topo.reset();
        info.m_fairmqTopology.reset();
        // We stop the session anyway if session ID is not nil.
        // Session can already be stopped by `dds-session stop` but session ID is not yet reset to nil.
        // If session is already stopped CSession::shutdown will reset pointers.
        if (info.m_session->getSessionID() != boost::uuids::nil_uuid()) {
            info.m_session->shutdown();
            if (info.m_session->getSessionID() == boost::uuids::nil_uuid()) {
                OLOG(info, common) << "DDS session shutted down";
            } else {
                fillError(common, error, ErrorCode::DDSShutdownSessionFailed, "Failed to shut down DDS session");
                return false;
            }
        }
    } catch (exception& _e) {
        fillError(common, error, ErrorCode::DDSShutdownSessionFailed, toString("Shutdown failed: ", _e.what()));
        return false;
    }
    return true;
}

bool ControlService::createTopo(const SCommonParams& common, SError& error, const std::string& _topologyFile)
{
    try {
        auto& info = getOrCreateSessionInfo(common);
        info.m_topo = make_unique<dds::topology_api::CTopology>(_topologyFile);
        OLOG(info, common) << "DDS topology " << std::quoted(_topologyFile) << " created successfully";
    } catch (exception& _e) {
        fillError(common, error, ErrorCode::DDSCreateTopologyFailed, toString("Failed to initialize DDS topology: ", _e.what()));
        return false;
    }
    return true;
}

bool ControlService::resetFairMQTopo(const SCommonParams& common)
{
    auto& info = getOrCreateSessionInfo(common);
    info.m_fairmqTopology.reset();
    return true;
}

bool ControlService::createFairMQTopo(const SCommonParams& common, SError& error, const std::string& _topologyFile)
{
    auto& info = getOrCreateSessionInfo(common);
    try {
        info.m_fairmqTopology.reset();
        info.m_fairmqTopology = make_unique<Topology>(dds::topology_api::CTopology(_topologyFile), info.m_session);
    } catch (exception& _e) {
        info.m_fairmqTopology = nullptr;
        fillError(common, error, ErrorCode::FairMQCreateTopologyFailed, toString("Failed to initialize FairMQ topology: ", _e.what()));
    }
    return info.m_fairmqTopology != nullptr;
}

bool ControlService::changeState(const SCommonParams& common,
                                         SError& error,
                                         TopologyTransition transition,
                                         const string& path,
                                         AggregatedTopologyState& aggregatedState,
                                         TopologyState* topologyState)
{
    auto& info = getOrCreateSessionInfo(common);
    if (info.m_fairmqTopology == nullptr) {
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    auto it{ expectedState.find(transition) };
    DeviceState _expectedState{ it != expectedState.end() ? it->second : DeviceState::Undefined };
    if (_expectedState == DeviceState::Undefined) {
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Unexpected FairMQ transition ", transition));
        return false;
    }

    bool success(true);

    try {
        auto result{ info.m_fairmqTopology->ChangeState(transition, path, requestTimeout(common)) };

        success = !result.first;
        if (success) {
            try {
                aggregatedState = AggregateState(result.second);
            } catch (exception& _e) {
                success = false;
                fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Aggregate topology state failed: ", _e.what()));
                OLOG(error, common) << stateSummaryString(common, result.second, _expectedState, info);
            }
            if (topologyState != nullptr) {
                fairMQToODCTopologyState(info.m_topo.get(), result.second, topologyState);
            }
        } else {
            switch (static_cast<ErrorCode>(result.first.value())) {
                case ErrorCode::OperationTimeout:
                    fillError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for change state ", transition));
                    break;
                default:
                    fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("FairMQ change state failed: ", result.first.message()));
                    break;
            }
            OLOG(error, common) << stateSummaryString(common, info.m_fairmqTopology->GetCurrentState(), _expectedState, info);
        }
    } catch (exception& _e) {
        success = false;
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", _e.what()));
        OLOG(error, common) << stateSummaryString(common, info.m_fairmqTopology->GetCurrentState(), _expectedState, info);
    }

    if (success) {
        OLOG(info, common) << "State changed to " << aggregatedState << " via " << transition << " transition";
    }

    const auto stats{ SStateStats(info.m_fairmqTopology->GetCurrentState()) };
    OLOG(info, common) << stats.tasksString();
    OLOG(info, common) << stats.collectionsString();

    return success;
}

bool ControlService::changeStateConfigure(const SCommonParams& common, SError& error, const string& path, AggregatedTopologyState& aggregatedState, TopologyState* topologyState)
{
    return changeState(common, error, TopologyTransition::InitDevice,   path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::CompleteInit, path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::Bind,         path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::Connect,      path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::InitTask,     path, aggregatedState, topologyState);
}

bool ControlService::changeStateReset(const SCommonParams& common, SError& error, const string& path, AggregatedTopologyState& aggregatedState, TopologyState* topologyState)
{
    return changeState(common, error, TopologyTransition::ResetTask, path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::ResetDevice, path, aggregatedState, topologyState);
}

void ControlService::updateTopologyOnFailure()
{

}

bool ControlService::getState(const SCommonParams& common, SError& error, const string& _path, AggregatedTopologyState& aggregatedState, TopologyState* _topologyState)
{
    auto& info = getOrCreateSessionInfo(common);
    if (info.m_fairmqTopology == nullptr) {
        error.m_code = MakeErrorCode(ErrorCode::FairMQGetStateFailed);
        error.m_details = "FairMQ topology is not initialized";
        return false;
    }

    bool success(true);
    auto const state(info.m_fairmqTopology->GetCurrentState());

    try {
        aggregatedState = aggregateStateForPath(info.m_topo.get(), state, _path);
    } catch (exception& _e) {
        success = false;
        fillError(common, error, ErrorCode::FairMQGetStateFailed, toString("Get state failed: ", _e.what()));
    }
    if (_topologyState != nullptr)
        fairMQToODCTopologyState(info.m_topo.get(), state, _topologyState);

    const auto stats{ SStateStats(state) };
    OLOG(info, common) << stats.tasksString();
    OLOG(info, common) << stats.collectionsString();

    return success;
}

bool ControlService::setProperties(const SCommonParams& common, SError& error, const SSetPropertiesParams& _params)
{
    auto& info = getOrCreateSessionInfo(common);
    if (info.m_fairmqTopology == nullptr) {
        fillError(common, error, ErrorCode::FairMQSetPropertiesFailed, "FairMQ topology is not initialized");
        return false;
    }

    bool success(true);

    try {
        auto result{ info.m_fairmqTopology->SetProperties(_params.m_properties, _params.m_path, requestTimeout(common)) };

        success = !result.first;
        if (success) {
            OLOG(info, common) << "Set property finished successfully";
        } else {
            switch (static_cast<ErrorCode>(result.first.value())) {
                case ErrorCode::OperationTimeout:
                    fillError(common, error, ErrorCode::RequestTimeout, toString("Timed out waiting for set property: ", result.first.message()));
                    break;
                default:
                    fillError(common, error, ErrorCode::FairMQSetPropertiesFailed, toString("Set property error message: ", result.first.message()));
                    break;
            }
            stringstream ss;
            size_t count{ 0 };
            ss << "List of failed devices for SetProperties request (" << result.second.size() << "):" << endl;
            for (auto taskId : result.second) {
                try {
                    TopoTaskInfo& taskInfo = info.getFromTaskCache(taskId);
                    ss << right << setw(7) << count << "   Task: " << taskInfo << endl;
                } catch (const exception& _e) {
                    OLOG(error, common) << "Set properties error: " << _e.what();
                }
                count++;
            }
            OLOG(error, common) << ss.str();
        }
    } catch (exception& _e) {
        success = false;
        fillError(common, error, ErrorCode::FairMQSetPropertiesFailed, toString("Set properties failed: ", _e.what()));
    }

    return success;
}

AggregatedTopologyState ControlService::aggregateStateForPath(const dds::topology_api::CTopology* _topo, const FairMQTopologyState& _topoState, const string& _path)
{
    if (_path.empty())
        return AggregateState(_topoState);

    if (_topo == nullptr)
        throw runtime_error("DDS topology is not initialized");

    try {
        // Check if path points to a single task
        // If task is found than return it's state as an aggregated topology state

        // Throws if task not found for path
        const auto& task{ _topo->getRuntimeTask(_path) };
        auto it{ find_if(_topoState.cbegin(), _topoState.cend(), [&](const FairMQTopologyState::value_type& _v) { return _v.taskId == task.m_taskId; }) };
        if (it != _topoState.cend())
            return static_cast<AggregatedTopologyState>(it->state);

        throw runtime_error("Device not found for path " + _path);
    } catch (exception& _e) {
        // In case of exception check that path contains multiple tasks

        // Collect all task IDs to a set for fast search
        set<Id_t> taskIds;
        auto it{ _topo->getRuntimeTaskIteratorMatchingPath(_path) };
        for_each(it.first, it.second, [&](const STopoRuntimeTask::FilterIterator_t::value_type& _v) { taskIds.insert(_v.second.m_taskId); });

        if (taskIds.empty())
            throw runtime_error("No tasks found matching the path " + _path);

        // Find a state of a first task
        auto firstIt{ find_if(_topoState.cbegin(), _topoState.cend(), [&](const FairMQTopologyState::value_type& _v) { return _v.taskId == *(taskIds.begin()); }) };
        if (firstIt == _topoState.cend())
            throw runtime_error("No states found for path " + _path);

        // Check that all selected devices have the same state
        AggregatedTopologyState first{ static_cast<AggregatedTopologyState>(firstIt->state) };
        if (std::all_of(_topoState.cbegin(), _topoState.cend(), [&](const FairMQTopologyState::value_type& _v) { return (taskIds.count(_v.taskId) > 0) ? _v.state == first : true; })) {
            return first;
        }

        return AggregatedTopologyState::Mixed;
    }
}

void ControlService::fairMQToODCTopologyState(const dds::topology_api::CTopology* _topo, const FairMQTopologyState& _fairmq, TopologyState* _odc)
{
    if (_odc == nullptr || _topo == nullptr)
        return;

    _odc->reserve(_fairmq.size());
    for (const auto& state : _fairmq) {
        const auto& task = _topo->getRuntimeTaskById(state.taskId);
        _odc->push_back(SDeviceStatus(state, task.m_taskPath));
    }
}

void ControlService::fillError(const SCommonParams& common, SError& error, ErrorCode errorCode, const string& msg)
{
    error.m_code = MakeErrorCode(errorCode);
    error.m_details = msg;
    OLOG(error, common) << error;
}

void ControlService::fillFatalError(const SCommonParams& common, SError& error, ErrorCode errorCode, const string& msg)
{
    error.m_code = MakeErrorCode(errorCode);
    error.m_details = msg;
    stringstream ss(error.m_details);
    std::string line;
    OLOG(fatal, common) << error.m_code;
    while (std::getline(ss, line, '\n')) {
        OLOG(fatal, common) << line;
    }
}

ControlService::SessionInfo& ControlService::getOrCreateSessionInfo(const SCommonParams& common)
{
    lock_guard<mutex> lock(mSessionsMtx);
    auto it = m_sessions.find(common.m_partitionID);
    if (it == m_sessions.end()) {
        auto newSessionInfo = make_unique<SessionInfo>();
        newSessionInfo->m_session = make_unique<CSession>();
        newSessionInfo->m_partitionID = common.m_partitionID;
        auto ret = m_sessions.emplace(common.m_partitionID, std::move(newSessionInfo));
        OLOG(debug, common) << "Return new session info for partition ID " << quoted(common.m_partitionID);
        return *(ret.first->second);
    }
    OLOG(debug, common) << "Return existing session info for partition ID " << quoted(common.m_partitionID);
    return *(it->second);
}

SError ControlService::checkSessionIsRunning(const SCommonParams& common, ErrorCode errorCode)
{
    SError error;
    auto& info = getOrCreateSessionInfo(common);
    if (!info.m_session->IsRunning()) {
        fillError(common, error, errorCode, "DDS session is not running. Use Init or Run to start the session.");
    }
    return error;
}

string ControlService::stateSummaryString(const SCommonParams& common, const FairMQTopologyState& _topologyState, DeviceState _expectedState, SessionInfo& sessionInfo)
{
    size_t taskTotalCount{ _topologyState.size() };
    size_t taskFailedCount{ 0 };
    stringstream ss;
    for (const auto& status : _topologyState) {
        // Print only failed devices
        if (status.state == _expectedState)
            continue;

        taskFailedCount++;
        if (taskFailedCount == 1) {
            ss << "List of failed devices for an expected state " << _expectedState << ":";
        }
        ss << endl
           << right << setw(7) << taskFailedCount << " Device: state (" << status.state << "), last state (" << status.lastState << "), task ID (" << status.taskId << "), collection ID ("
           << status.collectionId << "), "
           << "subscribed (" << status.subscribed_to_state_changes << ")";

        try {
            TopoTaskInfo& taskInfo = sessionInfo.getFromTaskCache(status.taskId);
            ss << ", " << taskInfo;
        } catch (const exception& _e) {
            OLOG(error, common) << "State summary error: " << _e.what();
        }
    }

    auto collectionMap{ GroupByCollectionId(_topologyState) };
    size_t collectionTotalCount{ collectionMap.size() };
    size_t collectionFailedCount{ 0 };
    for (const auto& states : collectionMap) {
        auto collectionState{ AggregateState(states.second) };
        auto collectionId{ states.first };
        // Print only failed collections
        if (collectionState == _expectedState)
            continue;

        collectionFailedCount++;
        if (collectionFailedCount == 1) {
            ss << endl << "List of failed collections for an expected state " << _expectedState << ":";
        }
        ss << endl << right << setw(7) << collectionFailedCount << "   Collection: state (" << collectionState << ")";

        try {
            TopoCollectionInfo& collectionInfo = sessionInfo.getFromCollectionCache(collectionId);
            ss << ", " << collectionInfo;
        } catch (const exception& _e) {
            OLOG(error, common) << "State summary error: " << _e.what();
        }
    }

    size_t taskSuccessCount{ taskTotalCount - taskFailedCount };
    size_t collectionSuccessCount{ collectionTotalCount - collectionFailedCount };
    ss << endl
       << "Summary for expected state (" << _expectedState << "): " << endl
       << "   Tasks total/success/failed (" << taskTotalCount << "/" << taskSuccessCount << "/" << taskFailedCount << ")" << endl
       << "   Collections total/success/failed (" << collectionTotalCount << "/" << collectionSuccessCount << "/" << collectionFailedCount << ")";

    return ss.str();
}

bool ControlService::subscribeToDDSSession(const SCommonParams& common, SError& error)
{
    try {
        auto& info = getOrCreateSessionInfo(common);
        if (info.m_session->IsRunning()) {
            // Subscrube on TaskDone events
            auto request{ SOnTaskDoneRequest::makeRequest(SOnTaskDoneRequest::request_t()) };
            request->setResponseCallback([common](const SOnTaskDoneResponseData& _info) {
                stringstream ss;
                ss << "Task (" << _info.m_taskID << ") with path (" << _info.m_taskPath << ") exited with code (" << _info.m_exitCode << ") and signal (" << _info.m_signal << ") on ("
                   << _info.m_host << ") in working directory (" << _info.m_wrkDir << ")";
                if (_info.m_exitCode != 0 || _info.m_signal != 0) {
                    OLOG(fatal, common) << ss.str();
                } else {
                    OLOG(debug, common) << ss.str();
                }
            });
            info.m_session->sendRequest<SOnTaskDoneRequest>(request);
            OLOG(info, common) << "Subscribed to task done event from session " << quoted(to_string(info.m_session->getSessionID()));
        } else {
            fillError(common, error, ErrorCode::DDSSubscribeToSessionFailed, "Failed to subscribe to task done events: session is not running");
            return false;
        }
    } catch (exception& _e) {
        fillError(common, error, ErrorCode::DDSSubscribeToSessionFailed, string("Failed to subscribe to task done events: ") + _e.what());
        return false;
    }
    return true;
}

chrono::seconds ControlService::requestTimeout(const SCommonParams& common) const
{
    auto timeout{ (common.m_timeout == 0) ? m_timeout : chrono::seconds(common.m_timeout) };
    OLOG(debug, common) << "Request timeout: " << timeout.count() << " sec";
    return timeout;
}

//
// ControlService
//

void ControlService::registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap)
{
    for (const auto& v : _pluginMap) {
        m_submit.registerPlugin(v.first, v.second);
    }
}

void ControlService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap)
{
    const set<string> avail{ "Initialize", "Submit", "Activate", "Run", "Update", "Configure", "SetProperties", "GetState", "Start", "Stop", "Reset", "Terminate", "Shutdown", "Status" };
    for (const auto& v : _triggerMap) {
        if (avail.count(v.first) == 0) {
            throw runtime_error(toString("Failed to add request trigger ", quoted(v.first), ". Invalid request name. Valid names are: ", quoted(boost::algorithm::join(avail, ", "))));
        }
        m_triggers.registerPlugin(v.first, v.second);
    }
}

void ControlService::restore(const std::string& _id)
{
    m_restoreId = _id;

    OLOG(info) << "Restoring sessions for " << quoted(_id);
    auto data{ CRestoreFile(_id).read() };
    for (const auto& v : data.m_partitions) {
        OLOG(info, v.m_partitionId, 0) << "Restoring (" << quoted(v.m_partitionId) << "/" << quoted(v.m_sessionId) << ")";
        auto result{ execInitialize(SCommonParams(v.m_partitionId, 0, 0), SInitializeParams(v.m_sessionId)) };
        if (result.m_error.m_code) {
            OLOG(info, v.m_partitionId, 0) << "Failed to attach to the session. Executing Shutdown trigger for (" << quoted(v.m_partitionId) << "/" << quoted(v.m_sessionId) << ")";
            execRequestTrigger("Shutdown", SCommonParams(v.m_partitionId, 0, 0));
        } else {
            OLOG(info, v.m_partitionId, 0) << "Successfully attached to the session (" << quoted(v.m_partitionId) << "/" << quoted(v.m_sessionId) << ")";
        }
    }
}

RequestResult ControlService::execInitialize(const SCommonParams& common, const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error;
    if (_params.m_sessionID.empty()) {
        // Shutdown DDS session if it is running already
        // Create new DDS session
        shutdownDDSSession(common, error) && createDDSSession(common, error) && subscribeToDDSSession(common, error);
    } else {
        // Shutdown DDS session if it is running already
        // Attach to an existing DDS session
        bool success{ shutdownDDSSession(common, error) && attachToDDSSession(common, error, _params.m_sessionID) && subscribeToDDSSession(common, error) };

        // Request current active topology, if any
        // If topology is active, create DDS and FairMQ topology
        if (success) {
            SCommanderInfoRequest::response_t commanderInfo;
            requestCommanderInfo(common, error, commanderInfo) && !commanderInfo.m_activeTopologyPath.empty() && createTopo(common, error, commanderInfo.m_activeTopologyPath)
                && createFairMQTopo(common, error, commanderInfo.m_activeTopologyPath);
        }
    }
    execRequestTrigger("Initialize", common);
    updateRestore();
    return createRequestResult(common, error, "Initialize done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult ControlService::execSubmit(const SCommonParams& common, const SSubmitParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error{ checkSessionIsRunning(common, ErrorCode::DDSSubmitAgentsFailed) };

    // Get DDS submit parameters from ODC resource plugin
    std::vector<CDDSSubmit::SParams> ddsParams;
    if (!error.m_code) {
        try {
            ddsParams = m_submit.makeParams(_params.m_plugin, _params.m_resources, common.m_partitionID, common.m_runNr);
        } catch (exception& _e) {
            fillError(common, error, ErrorCode::ResourcePluginFailed, toString("Resource plugin failed: ", _e.what()));
        }
    }

    OLOG(info) << "Preparing to submit " << ddsParams.size() << " configurations";

    if (!error.m_code) {
        size_t totalRequiredSlots = 0;
        for (const auto& param : ddsParams) {
            // OLOG(info) << "Submitting " << param;
            // Submit DDS agents
            if (submitDDSAgents(common, error, param)) {
                totalRequiredSlots += param.m_numRequiredSlots;
                OLOG(info) << "Done waiting for slots.";
            } else {
                OLOG(error) << "Submission failed";
                break;
            }
        }
        if (!error.m_code) {
            OLOG(info) << "Waiting for " << totalRequiredSlots << " slots...";
            waitForNumActiveAgents(common, error, totalRequiredSlots);
        }
    }
    execRequestTrigger("Submit", common);
    return createRequestResult(common, error, "Submit done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult ControlService::execActivate(const SCommonParams& common, const SActivateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Activate DDS topology
    // Create Topology
    SError error{ checkSessionIsRunning(common, ErrorCode::DDSActivateTopologyFailed) };
    if (!error.m_code) {
        string topo;
        try {
            topo = topoFilepath(common, _params);
        } catch (exception& _e) {
            fillFatalError(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", _e.what()));
        }
        if (!error.m_code) {
            activateDDSTopology(common, error, topo, STopologyRequest::request_t::EUpdateType::ACTIVATE) && createTopo(common, error, topo) && createFairMQTopo(common, error, topo);
        }
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle : AggregatedTopologyState::Undefined };
    execRequestTrigger("Activate", common);
    return createRequestResult(common, error, "Activate done", measure.duration(), state);
}

RequestResult ControlService::execRun(const SCommonParams& common, const SInitializeParams& _initializeParams, const SSubmitParams& _submitParams, const SActivateParams& _activateParams)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Run request doesn't support attachment to a DDS session.
    // Execute consecuently Initialize, Submit and Activate.
    SError error;
    if (!_initializeParams.m_sessionID.empty()) {
        error = SError(MakeErrorCode(ErrorCode::RequestNotSupported), "Attachment to a DDS session not supported");
    } else {
        error = execInitialize(common, _initializeParams).m_error;
        if (!error.m_code) {
            error = execSubmit(common, _submitParams).m_error;
            if (!error.m_code) {
                error = execActivate(common, _activateParams).m_error;
            }
        }
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle : AggregatedTopologyState::Undefined };
    execRequestTrigger("Run", common);
    return createRequestResult(common, error, "Run done", measure.duration(), state);
}

RequestResult ControlService::execUpdate(const SCommonParams& common, const SUpdateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    // Reset devices' state
    // Update DDS topology
    // Create Topology
    // Configure devices' state
    SError error;

    string topo;
    try {
        topo = topoFilepath(common, _params);
    } catch (exception& _e) {
        fillFatalError(common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", _e.what()));
    }

    if (!error.m_code) {
        changeStateReset(common, error, "", state) &&
        resetFairMQTopo(common) &&
        activateDDSTopology(common, error, topo, STopologyRequest::request_t::EUpdateType::UPDATE) &&
        createTopo(common, error, topo) &&
        createFairMQTopo(common, error, topo) &&
        changeStateConfigure(common, error, "", state);
    }
    execRequestTrigger("Update", common);
    return createRequestResult(common, error, "Update done", measure.duration(), state);
}

RequestResult ControlService::execShutdown(const SCommonParams& common)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    shutdownDDSSession(common, error);
    execRequestTrigger("Shutdown", common);
    return createRequestResult(common, error, "Shutdown done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult ControlService::execSetProperties(const SCommonParams& common, const SSetPropertiesParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    setProperties(common, error, _params);
    execRequestTrigger("SetProperties", common);
    return createRequestResult(common, error, "SetProperties done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult ControlService::execGetState(const SCommonParams& common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    getState(common, error, _params.m_path, state, fullState.get());
    execRequestTrigger("GetState", common);
    return createRequestResult(common, error, "GetState done", measure.duration(), state, std::move(fullState));
}

RequestResult ControlService::execConfigure(const SCommonParams& common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeStateConfigure(common, error, _params.m_path, state, fullState.get());
    execRequestTrigger("Configure", common);
    return createRequestResult(common, error, "Configure done", measure.duration(), state, std::move(fullState));
}

RequestResult ControlService::execStart(const SCommonParams& common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeState(common, error, TopologyTransition::Run, _params.m_path, state, fullState.get());
    execRequestTrigger("Start", common);
    return createRequestResult(common, error, "Start done", measure.duration(), state, std::move(fullState));
}

RequestResult ControlService::execStop(const SCommonParams& common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeState(common, error, TopologyTransition::Stop, _params.m_path, state, fullState.get());
    execRequestTrigger("Stop", common);
    return createRequestResult(common, error, "Stop done", measure.duration(), state, std::move(fullState));
}

RequestResult ControlService::execReset(const SCommonParams& common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeStateReset(common, error, _params.m_path, state, fullState.get());
    execRequestTrigger("Reset", common);
    return createRequestResult(common, error, "Reset done", measure.duration(), state, std::move(fullState));
}

RequestResult ControlService::execTerminate(const SCommonParams& common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeState(common, error, TopologyTransition::End, _params.m_path, state, fullState.get());
    execRequestTrigger("Terminate", common);
    return createRequestResult(common, error, "Terminate done", measure.duration(), state, std::move(fullState));
}

StatusRequestResult ControlService::execStatus(const SStatusParams& params)
{
    lock_guard<mutex> lock(mSessionsMtx);
    STimeMeasure<std::chrono::milliseconds> measure;
    StatusRequestResult result;
    for (const auto& v : m_sessions) {
        const auto& info{ v.second };
        SPartitionStatus status;
        status.m_partitionID = info->m_partitionID;
        try {
            status.m_sessionID = to_string(info->m_session->getSessionID());
            status.m_sessionStatus = (info->m_session->IsRunning()) ? ESessionStatus::running : ESessionStatus::stopped;
        } catch (exception& _e) {
            OLOG(warning, status.m_partitionID, 0) << "Failed to get session ID or session status: " << _e.what();
        }

        // Filter running sessions if needed
        if ((params.m_running && status.m_sessionStatus == ESessionStatus::running) || (!params.m_running)) {
            try {
                status.m_aggregatedState = (info->m_fairmqTopology != nullptr && info->m_topo != nullptr)
                ?
                aggregateStateForPath(info->m_topo.get(), info->m_fairmqTopology->GetCurrentState(), "")
                :
                AggregatedTopologyState::Undefined;
            } catch (exception& _e) {
                OLOG(warning, status.m_partitionID, 0) << "Failed to get an aggregated state: " << _e.what();
            }
            result.m_partitions.push_back(status);
        }
    }
    result.m_statusCode = EStatusCode::ok;
    result.m_msg = "Status done";
    result.m_execTime = measure.duration();
    execRequestTrigger("Status", SCommonParams());
    return result;
}
