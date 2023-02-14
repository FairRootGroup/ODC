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

/**
 * @brief Represents a DDS collection
 */
class DDSCollection
{
  public:
    using Id = std::uint64_t;

    explicit DDSCollection(Id id) : fId(id) {}

    Id GetId() const { return fId; }

    friend std::ostream& operator<<(std::ostream& os, const DDSCollection& collection) { return os << "DDSCollection id: " << collection.fId; }

  private:
    Id fId;
};

/**
 * @brief Represents a DDS task
 */
class DDSTask
{
  public:
    using Id = std::uint64_t;

    explicit DDSTask(Id id, Id collectionId)
        : fId(id)
        , fCollectionId(collectionId)
    {}

    Id GetId() const { return fId; }
    DDSCollection::Id GetCollectionId() const { return fCollectionId; }

    friend std::ostream& operator<<(std::ostream& os, const DDSTask& task) { return os << "DDSTask id: " << task.fId << ", collection id: " << task.fCollectionId; }

  private:
    Id fId;
    DDSCollection::Id fCollectionId;
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
    bool ignored;
    bool subscribedToStateChanges;
    DeviceState lastState;
    DeviceState state;
    DDSTask::Id taskId;
    DDSCollection::Id collectionId;
    int exitCode;
    int signal;
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
    // get the state of a first not-ignored device
    for (const auto& device : topoState) {
        if (!device.ignored) {
            state = static_cast<AggregatedState>(device.state);
            break;
        }
    }

    bool homogeneous = std::all_of(topoState.cbegin(), topoState.cend(), [&](DeviceStatus ds) {
        // if device is ignored, count it as homogeneous regardless of its actual state
        return ds.ignored ? true : (ds.state == state);
    });

    if (homogeneous) {
        return static_cast<AggregatedState>(state);
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
                  << ", agentID: "      << td.mAgentID
                  << ", slotID: "       << td.mSlotID
                  << ", collectionID: " << td.mCollectionID
                  << ", path: "         << td.mPath
                  << ", host: "         << td.mHost
                  << ", wrkDir: "       << td.mWrkDir;
    }
};

struct CollectionDetails
{
    uint64_t mAgentID = 0;       ///< Agent ID
    uint64_t mSlotID = 0;        ///< Slot ID
    uint64_t mCollectionID = 0;  ///< Collection ID
    std::string mPath;           ///< Path in the topology
    std::string mHost;           ///< Hostname
    std::string mWrkDir;         ///< Wrk directory

    friend std::ostream& operator<<(std::ostream& os, const CollectionDetails& cd)
    {
        return os << "collectionID: " << cd.mCollectionID
                  << ", agentID: "    << cd.mAgentID
                  << ", slotID: "     << cd.mSlotID
                  << ", path: "       << cd.mPath
                  << ", host: "       << cd.mHost
                  << ", wrkDir: "     << cd.mWrkDir;
    }
};

struct AgentDetails
{
    uint64_t mID = 0;
    std::string mZone;
    std::string mGroup;
    std::string mHost;
    std::string mPath;
    uint32_t mNumSlots = 0;

    friend std::ostream& operator<<(std::ostream& os, const AgentDetails& ad)
    {
        return os << "agentID: "    << ad.mID
                  << ", zone: "     << ad.mZone
                  << ", group: "    << ad.mGroup
                  << ", host: "     << ad.mHost
                  << ", path: "     << ad.mPath
                  << ", numSlots: " << ad.mNumSlots;
    }
};

struct CollectionNInfo
{
    int32_t nOriginal;
    int32_t nCurrent;
    int32_t nMin;
    std::string agentGroup;
};

struct FailedTasksCollections
{
    bool recoverable = true;
    std::vector<TaskDetails*> tasks;
    std::vector<CollectionDetails*> collections;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYDEFS */
