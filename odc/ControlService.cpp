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

void CControlService::execRequestTrigger(const string& _plugin, const SCommonParams& _common)
{
    if (m_triggers.isPluginRegistered(_plugin)) {
        try {
            OLOG(debug, _common) << "Executing request trigger " << quoted(_plugin);
            string out{ m_triggers.execPlugin(_plugin, "", _common.m_partitionID, _common.m_runNr) };
            OLOG(debug, _common) << "Request trigger " << quoted(_plugin) << " done: " << out;
        } catch (exception& _e) {
            OLOG(error, _common) << "Request trigger " << quoted(_plugin) << " failed: " << _e.what();
        }
    } else {
        OLOG(debug, _common) << "No plugins registered for " << quoted(_plugin);
    }
}

void CControlService::updateRestore()
{
    if (m_restoreId.empty()) {
        return;
    }

    OLOG(info) << "Updating restore file " << quoted(m_restoreId) << "...";
    SRestoreData data;

    lock_guard<mutex> lock(m_sessionsMutex);
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

    // Write the the file is locked by m_sessionsMutex
    // This is done in order to prevent write failure in case of a parallel execution.
    CRestoreFile(m_restoreId, data).write();
}

RequestResult CControlService::createRequestResult(const SCommonParams& _common,
                                                const SError& _error,
                                                const std::string& _msg,
                                                size_t _execTime,
                                                AggregatedTopologyState _aggregatedState,
                                                std::unique_ptr<TopologyState> fullState/*  = nullptr */)
{
    OLOG(debug, _common) << "Creating return value...";
    auto info{ getOrCreateSessionInfo(_common) };
    string sidStr{ to_string(info->m_session->getSessionID()) };
    EStatusCode status{ _error.m_code ? EStatusCode::error : EStatusCode::ok };
    return RequestResult(status, _msg, _execTime, _error, _common.m_partitionID, _common.m_runNr, sidStr, _aggregatedState, std::move(fullState));
}

bool CControlService::createDDSSession(const SCommonParams& _common, SError& _error)
{
    try {
        auto info{ getOrCreateSessionInfo(_common) };
        boost::uuids::uuid sessionID{ info->m_session->create() };
        OLOG(info, _common) << "DDS session created with session ID: " << to_string(sessionID);
    } catch (exception& _e) {
        fillError(_common, _error, ErrorCode::DDSCreateSessionFailed, toString("Failed to create a DDS session: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::attachToDDSSession(const SCommonParams& _common, SError& _error, const std::string& _sessionID)
{
    try {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_session->attach(_sessionID);
        OLOG(info, _common) << "Attach to a DDS session with session ID: " << _sessionID;
    } catch (exception& _e) {
        fillError(_common, _error, ErrorCode::DDSAttachToSessionFailed, toString("Failed to attach to a DDS session: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::submitDDSAgents(const SCommonParams& _common, SError& _error, const CDDSSubmit::SParams& _params)
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

    requestPtr->setMessageCallback([&success, &_error, &_common, this](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error) {
            success = false;
            fillError(_common, _error, ErrorCode::DDSSubmitAgentsFailed, toString("Submit error: ", _message.m_msg));
        } else {
            OLOG(info, _common) << "Submit: " << _message.m_msg;
        }
    });

    requestPtr->setDoneCallback([&cv, &_common]() {
        OLOG(info, _common) << "Agent submission done";
        cv.notify_all();
    });

    auto info{ getOrCreateSessionInfo(_common) };
    info->m_session->sendRequest<SSubmitRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus = cv.wait_for(lock, requestTimeout(_common));

    if (waitStatus == std::cv_status::timeout) {
        success = false;
        fillError(_common, _error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
    } else {
        OLOG(info, _common) << "Agent submission done successfully";
    }
    return success;
}

bool CControlService::requestCommanderInfo(const SCommonParams& _common, SError& _error, SCommanderInfoRequest::response_t& _commanderInfo)
{
    try {
        stringstream ss;
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_session->syncSendRequest<SCommanderInfoRequest>(SCommanderInfoRequest::request_t(), _commanderInfo, requestTimeout(_common), &ss);
        OLOG(info, _common) << ss.str();
        OLOG(debug, _common) << "Commander info: " << _commanderInfo;
        return true;
    } catch (exception& _e) {
        fillError(_common, _error, ErrorCode::DDSCommanderInfoFailed, toString("Error getting DDS commander info: ", _e.what()));
        return false;
    }
}

bool CControlService::waitForNumActiveAgents(const SCommonParams& _common, SError& _error, size_t _numAgents)
{
    try {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_session->waitForNumAgents<CSession::EAgentState::active>(_numAgents, requestTimeout(_common));
    } catch (std::exception& _e) {
        fillError(_common, _error, ErrorCode::RequestTimeout, toString("Timeout waiting for DDS agents: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::activateDDSTopology(const SCommonParams& _common, SError& _error, const string& _topologyFile, STopologyRequest::request_t::EUpdateType _updateType)
{
    bool success(true);

    STopologyRequest::request_t topoInfo;
    topoInfo.m_topologyFile = _topologyFile;
    topoInfo.m_disableValidation = true;
    topoInfo.m_updateType = _updateType;

    std::condition_variable cv;
    auto sessionInfo{ getOrCreateSessionInfo(_common) };

    STopologyRequest::ptr_t requestPtr{ STopologyRequest::makeRequest(topoInfo) };

    requestPtr->setMessageCallback([&success, &_error, &_common, this](const SMessageResponseData& _message) {
        if (_message.m_severity == dds::intercom_api::EMsgSeverity::error) {
            success = false;
            fillError(_common, _error, ErrorCode::DDSActivateTopologyFailed, toString("Activate error: ", _message.m_msg));
        } else {
            OLOG(debug, _common) << "Activate: " << _message.m_msg;
        }
    });

    requestPtr->setProgressCallback([&_common](const SProgressResponseData& _progress) {
        uint32_t completed{ _progress.m_completed + _progress.m_errors };
        if (completed == _progress.m_total) {
            OLOG(info, _common) << "Activated tasks (" << _progress.m_completed << "), errors (" << _progress.m_errors << "), total (" << _progress.m_total << ")";
        }
    });

    requestPtr->setResponseCallback([&_common, sessionInfo](const STopologyResponseData& _info) {
        OLOG(debug, _common) << "Activate: " << _info;

        // We are not interested in stopped tasks
        if (_info.m_activated) {
            auto task(make_shared<STopoItemInfo>());
            task->m_agentID = _info.m_agentID;
            task->m_slotID = _info.m_slotID;
            task->m_itemID = _info.m_taskID;
            task->m_path = _info.m_path;
            task->m_host = _info.m_host;
            task->m_wrkDir = _info.m_wrkDir;
            sessionInfo->addToTaskCache(task);

            if (_info.m_collectionID > 0) {
                auto collection(make_shared<STopoItemInfo>(*task));
                collection->m_itemID = _info.m_collectionID;
                auto pos = collection->m_path.rfind('/');
                if (pos != std::string::npos) {
                    collection->m_path.erase(pos);
                }
                sessionInfo->addToCollectionCache(collection);
            }
        }
    });

    requestPtr->setDoneCallback([&cv]() { cv.notify_all(); });

    sessionInfo->m_session->sendRequest<STopologyRequest>(requestPtr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    std::cv_status waitStatus{ cv.wait_for(lock, requestTimeout(_common)) };

    if (waitStatus == std::cv_status::timeout) {
        success = false;
        fillError(_common, _error, ErrorCode::RequestTimeout, "Timed out waiting for agent submission");
        OLOG(error, _common) << _error;
    }

    OLOG(info, _common) << "Topology " << quoted(_topologyFile) << ((success) ? " activated successfully" : " failed to activate");
    return success;
}

bool CControlService::shutdownDDSSession(const SCommonParams& _common, SError& _error)
{
    try {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_topo.reset();
        info->m_fairmqTopology.reset();
        // We stop the session anyway if session ID is not nil.
        // Session can already be stopped by `dds-session stop` but session ID is not yet reset to nil.
        // If session is already stopped CSession::shutdown will reset pointers.
        if (info->m_session->getSessionID() != boost::uuids::nil_uuid()) {
            info->m_session->shutdown();
            if (info->m_session->getSessionID() == boost::uuids::nil_uuid()) {
                OLOG(info, _common) << "DDS session shutted down";
            } else {
                fillError(_common, _error, ErrorCode::DDSShutdownSessionFailed, "Failed to shut down DDS session");
                return false;
            }
        }
    } catch (exception& _e) {
        fillError(_common, _error, ErrorCode::DDSShutdownSessionFailed, toString("Shutdown failed: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::createTopo(const SCommonParams& _common, SError& _error, const std::string& _topologyFile)
{
    try {
        auto info{ getOrCreateSessionInfo(_common) };
        info->m_topo = make_unique<dds::topology_api::CTopology>(_topologyFile);
        OLOG(info, _common) << "DDS topology " << std::quoted(_topologyFile) << " created successfully";
    } catch (exception& _e) {
        fillError(_common, _error, ErrorCode::DDSCreateTopologyFailed, toString("Failed to initialize DDS topology: ", _e.what()));
        return false;
    }
    return true;
}

bool CControlService::resetFairMQTopo(const SCommonParams& _common)
{
    auto info{ getOrCreateSessionInfo(_common) };
    info->m_fairmqTopology.reset();
    return true;
}

bool CControlService::createFairMQTopo(const SCommonParams& _common, SError& _error, const std::string& _topologyFile)
{
    auto info{ getOrCreateSessionInfo(_common) };
    try {
        info->m_fairmqTopology.reset();
        info->m_fairmqTopology = make_unique<Topology>(dds::topology_api::CTopology(_topologyFile), info->m_session);
    } catch (exception& _e) {
        info->m_fairmqTopology = nullptr;
        fillError(_common, _error, ErrorCode::FairMQCreateTopologyFailed, toString("Failed to initialize FairMQ topology: ", _e.what()));
    }
    return info->m_fairmqTopology != nullptr;
}

bool CControlService::changeState(const SCommonParams& common,
                                         SError& error,
                                         TopologyTransition transition,
                                         const string& path,
                                         AggregatedTopologyState& aggregatedState,
                                         TopologyState* topologyState)
{
    auto info{ getOrCreateSessionInfo(common) };
    if (info->m_fairmqTopology == nullptr) {
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
        auto result{ info->m_fairmqTopology->ChangeState(transition, path, requestTimeout(common)) };

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
                fairMQToODCTopologyState(info->m_topo.get(), result.second, topologyState);
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
            OLOG(error, common) << stateSummaryString(common, info->m_fairmqTopology->GetCurrentState(), _expectedState, info);
        }
    } catch (exception& _e) {
        success = false;
        fillError(common, error, ErrorCode::FairMQChangeStateFailed, toString("Change state failed: ", _e.what()));
        OLOG(error, common) << stateSummaryString(common, info->m_fairmqTopology->GetCurrentState(), _expectedState, info);
    }

    if (success) {
        OLOG(info, common) << "State changed to " << aggregatedState << " via " << transition << " transition";
    }

    const auto stats{ SStateStats(info->m_fairmqTopology->GetCurrentState()) };
    OLOG(info, common) << stats.tasksString();
    OLOG(info, common) << stats.collectionsString();

    return success;
}

bool CControlService::changeStateConfigure(const SCommonParams& common, SError& error, const string& path, AggregatedTopologyState& aggregatedState, TopologyState* topologyState)
{
    return changeState(common, error, TopologyTransition::InitDevice,   path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::CompleteInit, path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::Bind,         path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::Connect,      path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::InitTask,     path, aggregatedState, topologyState);
}

bool CControlService::changeStateReset(const SCommonParams& common, SError& error, const string& path, AggregatedTopologyState& aggregatedState, TopologyState* topologyState)
{
    return changeState(common, error, TopologyTransition::ResetTask, path, aggregatedState, topologyState)
        && changeState(common, error, TopologyTransition::ResetDevice, path, aggregatedState, topologyState);
}

bool CControlService::getState(const SCommonParams& _common, SError& _error, const string& _path, AggregatedTopologyState& _aggregatedState, TopologyState* _topologyState)
{
    auto info{ getOrCreateSessionInfo(_common) };
    if (info->m_fairmqTopology == nullptr) {
        fillError(_common, _error, ErrorCode::FairMQGetStateFailed, "FairMQ topology is not initialized");
        return false;
    }

    bool success(true);
    auto const state(info->m_fairmqTopology->GetCurrentState());

    try {
        _aggregatedState = aggregateStateForPath(info->m_topo.get(), state, _path);
    } catch (exception& _e) {
        success = false;
        fillError(_common, _error, ErrorCode::FairMQGetStateFailed, toString("Get state failed: ", _e.what()));
    }
    if (_topologyState != nullptr)
        fairMQToODCTopologyState(info->m_topo.get(), state, _topologyState);

    const auto stats{ SStateStats(state) };
    OLOG(info, _common) << stats.tasksString();
    OLOG(info, _common) << stats.collectionsString();

    return success;
}

bool CControlService::setProperties(const SCommonParams& _common, SError& _error, const SSetPropertiesParams& _params)
{
    auto info{ getOrCreateSessionInfo(_common) };
    if (info->m_fairmqTopology == nullptr) {
        fillError(_common, _error, ErrorCode::FairMQSetPropertiesFailed, "FairMQ topology is not initialized");
        return false;
    }

    bool success(true);

    try {
        auto result{ info->m_fairmqTopology->SetProperties(_params.m_properties, _params.m_path, requestTimeout(_common)) };

        success = !result.first;
        if (success) {
            OLOG(info, _common) << "Set property finished successfully";
        } else {
            switch (static_cast<ErrorCode>(result.first.value())) {
                case ErrorCode::OperationTimeout:
                    fillError(_common, _error, ErrorCode::RequestTimeout, toString("Timed out waiting for set property: ", result.first.message()));
                    break;
                default:
                    fillError(_common, _error, ErrorCode::FairMQSetPropertiesFailed, toString("Set property error message: ", result.first.message()));
                    break;
            }
            stringstream ss;
            size_t count{ 0 };
            ss << "List of failed devices for SetProperties request (" << result.second.size() << "):" << endl;
            for (auto taskId : result.second) {
                try {
                    auto item{ info->getFromTaskCache(taskId) };
                    ss << right << setw(7) << count << "   Task: " << item->toString() << endl;
                } catch (const exception& _e) {
                    OLOG(error, _common) << "Set properties error: " << _e.what();
                }
                count++;
            }
            OLOG(error, _common) << ss.str();
        }
    } catch (exception& _e) {
        success = false;
        fillError(_common, _error, ErrorCode::FairMQSetPropertiesFailed, toString("Set properties failed: ", _e.what()));
    }

    return success;
}

AggregatedTopologyState CControlService::aggregateStateForPath(const dds::topology_api::CTopology* _topo, const FairMQTopologyState& _topoState, const string& _path)
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

void CControlService::fairMQToODCTopologyState(const dds::topology_api::CTopology* _topo, const FairMQTopologyState& _fairmq, TopologyState* _odc)
{
    if (_odc == nullptr || _topo == nullptr)
        return;

    _odc->reserve(_fairmq.size());
    for (const auto& state : _fairmq) {
        const auto& task = _topo->getRuntimeTaskById(state.taskId);
        _odc->push_back(SDeviceStatus(state, task.m_taskPath));
    }
}

void CControlService::fillError(const SCommonParams& _common, SError& _error, ErrorCode _errorCode, const string& _msg)
{
    _error.m_code = MakeErrorCode(_errorCode);
    _error.m_details = _msg;
    OLOG(error, _common) << _error;
}

void CControlService::fillFatalError(const SCommonParams& _common, SError& _error, ErrorCode _errorCode, const string& _msg)
{
    _error.m_code = MakeErrorCode(_errorCode);
    _error.m_details = _msg;
    stringstream ss(_error.m_details);
    std::string line;
    OLOG(fatal, _common) << _error.m_code;
    while (std::getline(ss, line, '\n')) {
        OLOG(fatal, _common) << line;
    }
}

CControlService::SSessionInfo::Ptr_t CControlService::getOrCreateSessionInfo(const SCommonParams& _common)
{
    const auto& prt{ _common.m_partitionID };
    lock_guard<mutex> lock(m_sessionsMutex);
    auto it{ m_sessions.find(prt) };
    if (it == m_sessions.end()) {
        auto newSessionInfo{ make_shared<SSessionInfo>() };
        newSessionInfo->m_session = make_shared<CSession>();
        newSessionInfo->m_partitionID = prt;
        m_sessions.insert(pair<std::string, SSessionInfo::Ptr_t>(prt, newSessionInfo));
        OLOG(debug, _common) << "Return new session info";
        return newSessionInfo;
    }
    OLOG(debug, _common) << "Return existing session info";
    return it->second;
}

SError CControlService::checkSessionIsRunning(const SCommonParams& _common, ErrorCode _errorCode)
{
    SError error;
    auto info{ getOrCreateSessionInfo(_common) };
    if (!info->m_session->IsRunning()) {
        fillError(_common, error, _errorCode, "DDS session is not running. Use Init or Run to start the session.");
    }
    return error;
}

string CControlService::stateSummaryString(const SCommonParams& _common, const FairMQTopologyState& _topologyState, DeviceState _expectedState, SSessionInfo::Ptr_t _info)
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
            auto item{ _info->getFromTaskCache(status.taskId) };
            ss << ", " << item->toString();
        } catch (const exception& _e) {
            OLOG(error, _common) << "State summary error: " << _e.what();
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
            auto item{ _info->getFromCollectionCache(collectionId) };
            ss << ", " << item->toString();
        } catch (const exception& _e) {
            OLOG(error, _common) << "State summary error: " << _e.what();
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

bool CControlService::subscribeToDDSSession(const SCommonParams& _common, SError& _error)
{
    try {
        auto info{ getOrCreateSessionInfo(_common) };
        if (info->m_session->IsRunning()) {
            // Subscrube on TaskDone events
            auto request{ SOnTaskDoneRequest::makeRequest(SOnTaskDoneRequest::request_t()) };
            request->setResponseCallback([_common](const SOnTaskDoneResponseData& _info) {
                stringstream ss;
                ss << "Task (" << _info.m_taskID << ") with path (" << _info.m_taskPath << ") exited with code (" << _info.m_exitCode << ") and signal (" << _info.m_signal << ") on ("
                   << _info.m_host << ") in working directory (" << _info.m_wrkDir << ")";
                if (_info.m_exitCode != 0 || _info.m_signal != 0) {
                    OLOG(fatal, _common) << ss.str();
                } else {
                    OLOG(debug, _common) << ss.str();
                }
            });
            info->m_session->sendRequest<SOnTaskDoneRequest>(request);
            OLOG(info, _common) << "Subscribed to task done event from session " << quoted(to_string(info->m_session->getSessionID()));
        } else {
            fillError(_common, _error, ErrorCode::DDSSubscribeToSessionFailed, "Failed to subscribe to task done events: session is not running");
            return false;
        }
    } catch (exception& _e) {
        fillError(_common, _error, ErrorCode::DDSSubscribeToSessionFailed, string("Failed to subscribe to task done events: ") + _e.what());
        return false;
    }
    return true;
}

chrono::seconds CControlService::requestTimeout(const SCommonParams& _common) const
{
    auto timeout{ (_common.m_timeout == 0) ? m_timeout : chrono::seconds(_common.m_timeout) };
    OLOG(debug, _common) << "Request timeout: " << timeout.count() << " sec";
    return timeout;
}

//
// CControlService
//

void CControlService::registerResourcePlugins(const CDDSSubmit::PluginMap_t& _pluginMap)
{
    for (const auto& v : _pluginMap) {
        m_submit.registerPlugin(v.first, v.second);
    }
}

void CControlService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap)
{
    const set<string> avail{ "Initialize", "Submit", "Activate", "Run", "Update", "Configure", "SetProperties", "GetState", "Start", "Stop", "Reset", "Terminate", "Shutdown", "Status" };
    for (const auto& v : _triggerMap) {
        if (avail.count(v.first) == 0) {
            throw runtime_error(toString("Failed to add request trigger ", quoted(v.first), ". Invalid request name. Valid names are: ", quoted(boost::algorithm::join(avail, ", "))));
        }
        m_triggers.registerPlugin(v.first, v.second);
    }
}

void CControlService::restore(const std::string& _id)
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

RequestResult CControlService::execInitialize(const SCommonParams& _common, const SInitializeParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error;
    if (_params.m_sessionID.empty()) {
        // Shutdown DDS session if it is running already
        // Create new DDS session
        shutdownDDSSession(_common, error) && createDDSSession(_common, error) && subscribeToDDSSession(_common, error);
    } else {
        // Shutdown DDS session if it is running already
        // Attach to an existing DDS session
        bool success{ shutdownDDSSession(_common, error) && attachToDDSSession(_common, error, _params.m_sessionID) && subscribeToDDSSession(_common, error) };

        // Request current active topology, if any
        // If topology is active, create DDS and FairMQ topology
        if (success) {
            SCommanderInfoRequest::response_t commanderInfo;
            requestCommanderInfo(_common, error, commanderInfo) && !commanderInfo.m_activeTopologyPath.empty() && createTopo(_common, error, commanderInfo.m_activeTopologyPath)
                && createFairMQTopo(_common, error, commanderInfo.m_activeTopologyPath);
        }
    }
    execRequestTrigger("Initialize", _common);
    updateRestore();
    return createRequestResult(_common, error, "Initialize done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult CControlService::execSubmit(const SCommonParams& _common, const SSubmitParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;

    SError error{ checkSessionIsRunning(_common, ErrorCode::DDSSubmitAgentsFailed) };

    // Get DDS submit parameters from ODC resource plugin
    std::vector<CDDSSubmit::SParams> ddsParams;
    if (!error.m_code) {
        try {
            ddsParams = m_submit.makeParams(_params.m_plugin, _params.m_resources, _common.m_partitionID, _common.m_runNr);
        } catch (exception& _e) {
            fillError(_common, error, ErrorCode::ResourcePluginFailed, toString("Resource plugin failed: ", _e.what()));
        }
    }

    OLOG(info) << "Preparing to submit " << ddsParams.size() << " configurations";

    if (!error.m_code) {
        size_t totalRequiredSlots = 0;
        for (const auto& param : ddsParams) {
            // OLOG(info) << "Submitting " << param;
            // Submit DDS agents
            if (submitDDSAgents(_common, error, param)) {
                totalRequiredSlots += param.m_numRequiredSlots;
                OLOG(info) << "Done waiting for slots.";
            } else {
                OLOG(error) << "Submission failed";
                break;
            }
        }
        if (!error.m_code) {
            OLOG(info) << "Waiting for " << totalRequiredSlots << " slots...";
            waitForNumActiveAgents(_common, error, totalRequiredSlots);
        }
    }
    execRequestTrigger("Submit", _common);
    return createRequestResult(_common, error, "Submit done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult CControlService::execActivate(const SCommonParams& _common, const SActivateParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Activate DDS topology
    // Create Topology
    SError error{ checkSessionIsRunning(_common, ErrorCode::DDSActivateTopologyFailed) };
    if (!error.m_code) {
        string topo;
        try {
            topo = topoFilepath(_common, _params);
        } catch (exception& _e) {
            fillFatalError(_common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", _e.what()));
        }
        if (!error.m_code) {
            activateDDSTopology(_common, error, topo, STopologyRequest::request_t::EUpdateType::ACTIVATE) && createTopo(_common, error, topo) && createFairMQTopo(_common, error, topo);
        }
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle : AggregatedTopologyState::Undefined };
    execRequestTrigger("Activate", _common);
    return createRequestResult(_common, error, "Activate done", measure.duration(), state);
}

RequestResult CControlService::execRun(const SCommonParams& _common, const SInitializeParams& _initializeParams, const SSubmitParams& _submitParams, const SActivateParams& _activateParams)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    // Run request doesn't support attachment to a DDS session.
    // Execute consecuently Initialize, Submit and Activate.
    SError error;
    if (!_initializeParams.m_sessionID.empty()) {
        error = SError(MakeErrorCode(ErrorCode::RequestNotSupported), "Attachment to a DDS session not supported");
    } else {
        error = execInitialize(_common, _initializeParams).m_error;
        if (!error.m_code) {
            error = execSubmit(_common, _submitParams).m_error;
            if (!error.m_code) {
                error = execActivate(_common, _activateParams).m_error;
            }
        }
    }
    AggregatedTopologyState state{ !error.m_code ? AggregatedTopologyState::Idle : AggregatedTopologyState::Undefined };
    execRequestTrigger("Run", _common);
    return createRequestResult(_common, error, "Run done", measure.duration(), state);
}

RequestResult CControlService::execUpdate(const SCommonParams& _common, const SUpdateParams& _params)
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
        topo = topoFilepath(_common, _params);
    } catch (exception& _e) {
        fillFatalError(_common, error, ErrorCode::TopologyFailed, toString("Incorrect topology provided: ", _e.what()));
    }

    if (!error.m_code) {
        changeStateReset(_common, error, "", state) &&
        resetFairMQTopo(_common) &&
        activateDDSTopology(_common, error, topo, STopologyRequest::request_t::EUpdateType::UPDATE) &&
        createTopo(_common, error, topo) &&
        createFairMQTopo(_common, error, topo) &&
        changeStateConfigure(_common, error, "", state);
    }
    execRequestTrigger("Update", _common);
    return createRequestResult(_common, error, "Update done", measure.duration(), state);
}

RequestResult CControlService::execShutdown(const SCommonParams& _common)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    shutdownDDSSession(_common, error);
    execRequestTrigger("Shutdown", _common);
    return createRequestResult(_common, error, "Shutdown done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult CControlService::execSetProperties(const SCommonParams& _common, const SSetPropertiesParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    SError error;
    setProperties(_common, error, _params);
    execRequestTrigger("SetProperties", _common);
    return createRequestResult(_common, error, "SetProperties done", measure.duration(), AggregatedTopologyState::Undefined);
}

RequestResult CControlService::execGetState(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    getState(_common, error, _params.m_path, state, fullState.get());
    execRequestTrigger("GetState", _common);
    return createRequestResult(_common, error, "GetState done", measure.duration(), state, std::move(fullState));
}

RequestResult CControlService::execConfigure(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeStateConfigure(_common, error, _params.m_path, state, fullState.get());
    execRequestTrigger("Configure", _common);
    return createRequestResult(_common, error, "Configure done", measure.duration(), state, std::move(fullState));
}

RequestResult CControlService::execStart(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeState(_common, error, TopologyTransition::Run, _params.m_path, state, fullState.get());
    execRequestTrigger("Start", _common);
    return createRequestResult(_common, error, "Start done", measure.duration(), state, std::move(fullState));
}

RequestResult CControlService::execStop(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeState(_common, error, TopologyTransition::Stop, _params.m_path, state, fullState.get());
    execRequestTrigger("Stop", _common);
    return createRequestResult(_common, error, "Stop done", measure.duration(), state, std::move(fullState));
}

RequestResult CControlService::execReset(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeStateReset(_common, error, _params.m_path, state, fullState.get());
    execRequestTrigger("Reset", _common);
    return createRequestResult(_common, error, "Reset done", measure.duration(), state, std::move(fullState));
}

RequestResult CControlService::execTerminate(const SCommonParams& _common, const SDeviceParams& _params)
{
    STimeMeasure<std::chrono::milliseconds> measure;
    AggregatedTopologyState state{ AggregatedTopologyState::Undefined };
    std::unique_ptr<TopologyState> fullState = _params.m_detailed ? make_unique<TopologyState>() : nullptr;
    SError error;
    changeState(_common, error, TopologyTransition::End, _params.m_path, state, fullState.get());
    execRequestTrigger("Terminate", _common);
    return createRequestResult(_common, error, "Terminate done", measure.duration(), state, std::move(fullState));
}

StatusRequestResult CControlService::execStatus(const SStatusParams& params)
{
    lock_guard<mutex> lock(m_sessionsMutex);
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
