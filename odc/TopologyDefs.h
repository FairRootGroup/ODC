/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_TOPOLOGYDEFS
#define ODC_TOPOLOGYDEFS

#include <fairmq/States.h>
#include <odc/cc/CustomCommands.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace odc::core
{

using DeviceId = std::string;
using DeviceState = fair::mq::State;
using DeviceTransition = fair::mq::Transition;

struct DDSCollection
{
    using Id = std::uint64_t;
};

struct DDSTask
{
    using Id = std::uint64_t;
};

static const std::map<DeviceTransition, DeviceState> gExpectedState = {
    { DeviceTransition::InitDevice,   DeviceState::InitializingDevice },
    { DeviceTransition::CompleteInit, DeviceState::Initialized        },
    { DeviceTransition::Bind,         DeviceState::Bound              },
    { DeviceTransition::Connect,      DeviceState::DeviceReady        },
    { DeviceTransition::InitTask,     DeviceState::Ready              },
    { DeviceTransition::Run,          DeviceState::Running            },
    { DeviceTransition::Stop,         DeviceState::Ready              },
    { DeviceTransition::ResetTask,    DeviceState::DeviceReady        },
    { DeviceTransition::ResetDevice,  DeviceState::Idle               },
    { DeviceTransition::End,          DeviceState::Exiting            }
};

// mirrors DeviceState, but adds a "Mixed" state that represents a topology where devices are currently not in the
// same state.
enum class AggregatedState : int
{
    Undefined          = static_cast<int>(fair::mq::State::Undefined),
    Ok                 = static_cast<int>(fair::mq::State::Ok),
    Error              = static_cast<int>(fair::mq::State::Error),
    Idle               = static_cast<int>(fair::mq::State::Idle),
    InitializingDevice = static_cast<int>(fair::mq::State::InitializingDevice),
    Initialized        = static_cast<int>(fair::mq::State::Initialized),
    Binding            = static_cast<int>(fair::mq::State::Binding),
    Bound              = static_cast<int>(fair::mq::State::Bound),
    Connecting         = static_cast<int>(fair::mq::State::Connecting),
    DeviceReady        = static_cast<int>(fair::mq::State::DeviceReady),
    InitializingTask   = static_cast<int>(fair::mq::State::InitializingTask),
    Ready              = static_cast<int>(fair::mq::State::Ready),
    Running            = static_cast<int>(fair::mq::State::Running),
    ResettingTask      = static_cast<int>(fair::mq::State::ResettingTask),
    ResettingDevice    = static_cast<int>(fair::mq::State::ResettingDevice),
    Exiting            = static_cast<int>(fair::mq::State::Exiting),
    Mixed
};

inline bool operator==(DeviceState lhs, AggregatedState rhs) { return static_cast<int>(lhs) == static_cast<int>(rhs); }

inline bool operator==(AggregatedState lhs, DeviceState rhs) { return static_cast<int>(lhs) == static_cast<int>(rhs); }

inline std::ostream& operator<<(std::ostream& os, const AggregatedState& state)
{
    if (state == AggregatedState::Mixed) {
        return os << "MIXED";
    } else {
        return os << static_cast<DeviceState>(state);
    }
}

inline std::string GetAggregatedStateName(AggregatedState s)
{
    if (s == AggregatedState::Mixed) {
        return "MIXED";
    } else {
        return fair::mq::GetStateName(static_cast<fair::mq::State>(s));
    }
}

inline AggregatedState GetAggregatedState(const std::string& state)
{
    if (state == "MIXED") {
        return AggregatedState::Mixed;
    } else {
        return static_cast<AggregatedState>(fair::mq::GetState(state));
    }
}

struct DeviceStatus
{
    DeviceStatus() = default;

    DeviceStatus(bool _ignored, bool _expendable, bool _subscribed, DeviceState _lastState, DeviceState _state, DDSTask::Id _taskId, DDSCollection::Id _collectionId, int _exitCode, int _signal)
        : ignored(_ignored)
        , expendable(_expendable)
        , subscribedToStateChanges(_subscribed)
        , lastState(_lastState)
        , state(_state)
        , taskId(_taskId)
        , collectionId(_collectionId)
        , exitCode(_exitCode)
        , signal(_signal)
    {}

    DeviceStatus(bool _expendable, DDSTask::Id _taskId, DDSCollection::Id _collectionId)
        : expendable(_expendable)
        , taskId(_taskId)
        , collectionId(_collectionId)
    {}

    bool ignored = false;
    bool expendable = false;
    bool subscribedToStateChanges = false;
    DeviceState lastState = DeviceState::Undefined;
    DeviceState state = DeviceState::Undefined;
    DDSTask::Id taskId;
    DDSCollection::Id collectionId;
    int exitCode = -1;
    int signal = -1;
};

struct DetailedTaskStatus
{
    DetailedTaskStatus() {}
    DetailedTaskStatus(const DeviceStatus& status, const std::string& path, const std::string& host)
        : mStatus(status)
        , mPath(path)
        , mHost(host)
    {}

    DeviceStatus mStatus;
    std::string mPath;
    std::string mHost;
};

using DetailedState = std::vector<DetailedTaskStatus>;

struct TopologyState
{
    TopologyState()
        : aggregated(AggregatedState::Undefined)
    {}
    TopologyState(AggregatedState _aggregated)
        : aggregated(_aggregated)
    {}
    TopologyState(AggregatedState _aggregated, std::optional<DetailedState> _detailed)
        : aggregated(_aggregated)
        , detailed(_detailed)
    {}

    AggregatedState aggregated;
    std::optional<DetailedState> detailed;
};

using DeviceProperty = std::pair<std::string, std::string>; /// pair := (key, value)
using DeviceProperties = std::vector<DeviceProperty>;
using FailedDevices = std::unordered_set<DDSTask::Id>;

struct GetPropertiesResult
{
    struct Device
    {
        DeviceProperties props;
    };
    std::unordered_map<DDSTask::Id, Device> devices;
    FailedDevices failed;
};

using TopoState = std::vector<DeviceStatus>;
using TopoStateIndex = std::unordered_map<DDSTask::Id, int>; //  task id -> index in the data vector
using TopoStateByTask = std::unordered_map<DDSTask::Id, DeviceStatus>;
using TopoStateByCollection = std::unordered_map<DDSCollection::Id, std::vector<DeviceStatus>>;
using TopoTransition = fair::mq::Transition;

inline AggregatedState AggregateState(const TopoState& topoState)
{
    AggregatedState state = AggregatedState::Mixed;
    bool homogeneous = true;
    // get the state of a first not-ignored device
    for (const auto& ds : topoState) {
        if (!ds.ignored) {
            if (state == AggregatedState::Mixed) {
                // first assignment
                state = static_cast<AggregatedState>(ds.state);
            } else {
                if (ds.state == DeviceState::Error) {
                    // if any device is in error state and it is not ignored, the whole topology is in the error state
                    return AggregatedState::Error;
                } else if (static_cast<AggregatedState>(ds.state) != state) {
                    homogeneous = false;
                }
            }
        }
    }

    if (homogeneous) {
        return state;
    }

    return AggregatedState::Mixed;
}

inline bool StateEqualsTo(const TopoState& topoState, DeviceState state) { return AggregateState(topoState) == static_cast<AggregatedState>(state); }

inline TopoStateByCollection GroupByCollectionId(const TopoState& topoState)
{
    TopoStateByCollection state;
    for (const auto& ds : topoState) {
        if (ds.collectionId != 0) {
            state[ds.collectionId].push_back(ds);
        }
    }

    return state;
}

inline TopoStateByTask GroupByTaskId(const TopoState& topoState)
{
    TopoStateByTask state;
    for (const auto& ds : topoState) {
        state[ds.taskId] = ds;
    }

    return state;
}

using Duration = std::chrono::microseconds;

struct TaskDetails
{
    uint64_t mAgentID = 0;       ///< Agent ID
    uint64_t mSlotID = 0;        ///< Slot ID
    uint64_t mTaskID = 0;        ///< Task ID
    uint64_t mCollectionID = 0;  ///< Collection ID, 0 if not assigned
    std::string mPath;           ///< Path in the topology
    std::string mHost;           ///< Hostname
    std::string mWrkDir;         ///< Wrk directory

    friend std::ostream& operator<<(std::ostream& os, const TaskDetails& td)
    {
        return os << "taskID: "         << td.mTaskID
                  << "; agentID: "      << td.mAgentID
                  << "; slotID: "       << td.mSlotID
                  << "; collectionID: " << td.mCollectionID
                  << "; path: "         << td.mPath
                  << "; host: "         << td.mHost
                  << "; wrkDir: "       << td.mWrkDir;
    }
};

struct CollectionDetails
{
    uint64_t mAgentID = 0;       ///< Agent ID
    uint64_t mCollectionID = 0;  ///< Collection ID
    std::string mPath;           ///< Path in the topology
    std::string mHost;           ///< Hostname
    std::string mWrkDir;         ///< Wrk directory

    friend std::ostream& operator<<(std::ostream& os, const CollectionDetails& cd)
    {
        return os << "collectionID: " << cd.mCollectionID
                  << "; agentID: "    << cd.mAgentID
                  << "; path: "       << cd.mPath
                  << "; host: "       << cd.mHost
                  << "; wrkDir: "     << cd.mWrkDir;
    }
};

struct CollectionNInfo
{
    int32_t nOriginal;
    int32_t nCurrent;
    int32_t nMin;
    std::string agentGroup;

    friend std::ostream& operator<<(std::ostream& os, const CollectionNInfo& cni)
    {
        return os << "n (original): "  << cni.nOriginal
                  << "; n (current): " << cni.nCurrent
                  << "; n (min): "     << cni.nMin
                  << "; agentGroup: "  << cni.agentGroup;
    }
};

struct CollectionInfo
{
    std::string name;
    std::string zone;
    std::string agentGroup;
    std::string topoParent;
    std::string topoPath;
    int32_t nOriginal;
    int32_t nCurrent;
    int32_t nMin;
    int nCores;
    int32_t numTasks;
    int32_t totalTasks;
    std::unordered_map<uint64_t, uint64_t> mRuntimeCollectionAgents; ///< runtime collection ID -> agent ID

    friend std::ostream& operator<<(std::ostream& os, const CollectionInfo& ci)
    {
        return os << "name: "           << std::quoted(ci.name)
                  << "; zone: "         << std::quoted(ci.zone)
                  << "; agent group: "  << std::quoted(ci.agentGroup)
                  << "; topo parent: "  << std::quoted(ci.topoParent)
                  << "; topo path: "    << std::quoted(ci.topoPath)
                  << "; n (original): " << ci.nOriginal
                  << "; n (current): "  << ci.nCurrent
                  << "; n (min): "      << ci.nMin
                  << "; nCores: "       << ci.nCores
                  << "; numTasks: "     << ci.numTasks
                  << "; totalTasks: "   << ci.totalTasks;
    }
};

struct TaskInfo
{
    std::string name;
    std::string zone;
    std::string agentGroup;
    std::string topoParent;
    int32_t n;

    friend std::ostream& operator<<(std::ostream& os, const TaskInfo& ti)
    {
        return os << "name: "          << std::quoted(ti.name)
                  << "; zone: "        << std::quoted(ti.zone)
                  << "; agent group: " << std::quoted(ti.agentGroup)
                  << "; topo parent: " << std::quoted(ti.topoParent)
                  << "; n: "           << ti.n;
    }
};

struct AgentGroupInfo
{
    std::string name;
    std::string zone;
    int32_t numAgents;
    int32_t minAgents;
    int32_t numSlots;
    int32_t numCores;

    friend std::ostream& operator<<(std::ostream& os, const AgentGroupInfo& agi)
    {
        return os << "name: "        << std::quoted(agi.name)
                  << "; zone: "      << std::quoted(agi.zone)
                  << "; numAgents: " << agi.numAgents
                  << "; minAgents: " << agi.minAgents
                  << "; numSlots: "  << agi.numSlots
                  << "; numCores: "  << agi.numCores;
    }
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYDEFS */
