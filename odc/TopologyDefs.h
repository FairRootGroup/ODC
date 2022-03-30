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

    friend auto operator<<(std::ostream& os, const DDSCollection& collection) -> std::ostream& { return os << "DDSCollection id: " << collection.fId; }

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
    {
    }

    Id GetId() const { return fId; }
    DDSCollection::Id GetCollectionId() const { return fCollectionId; }

    friend auto operator<<(std::ostream& os, const DDSTask& task) -> std::ostream& { return os << "DDSTask id: " << task.fId << ", collection id: " << task.fCollectionId; }

  private:
    Id fId;
    DDSCollection::Id fCollectionId;
};

static const std::map<DeviceTransition, DeviceState> gExpectedState = { { DeviceTransition::InitDevice, DeviceState::InitializingDevice },
                                                                        { DeviceTransition::CompleteInit, DeviceState::Initialized },
                                                                        { DeviceTransition::Bind, DeviceState::Bound },
                                                                        { DeviceTransition::Connect, DeviceState::DeviceReady },
                                                                        { DeviceTransition::InitTask, DeviceState::Ready },
                                                                        { DeviceTransition::Run, DeviceState::Running },
                                                                        { DeviceTransition::Stop, DeviceState::Ready },
                                                                        { DeviceTransition::ResetTask, DeviceState::DeviceReady },
                                                                        { DeviceTransition::ResetDevice, DeviceState::Idle },
                                                                        { DeviceTransition::End, DeviceState::Exiting } };

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

inline auto operator==(DeviceState lhs, AggregatedState rhs) -> bool { return static_cast<int>(lhs) == static_cast<int>(rhs); }

inline auto operator==(AggregatedState lhs, DeviceState rhs) -> bool { return static_cast<int>(lhs) == static_cast<int>(rhs); }

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
    bool subscribedToStateChanges;
    DeviceState lastState;
    DeviceState state;
    DDSTask::Id taskId;
    DDSCollection::Id collectionId;
    int exitCode;
    int signal;
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

using TopologyState = std::vector<DeviceStatus>;
using TopologyStateIndex = std::unordered_map<DDSTask::Id, int>; //  task id -> index in the data vector
using TopologyStateByTask = std::unordered_map<DDSTask::Id, DeviceStatus>;
using TopologyStateByCollection = std::unordered_map<DDSCollection::Id, std::vector<DeviceStatus>>;
using TopologyTransition = fair::mq::Transition;

inline AggregatedState AggregateState(const TopologyState& topoState)
{
    DeviceState first = topoState.begin()->state;

    bool homogeneous = std::all_of(topoState.cbegin(), topoState.cend(), [&](TopologyState::value_type i) {
        return i.state == first;
    });

    if (homogeneous) {
        return static_cast<AggregatedState>(first);
    }

    return AggregatedState::Mixed;
}

inline bool StateEqualsTo(const TopologyState& topologyState, DeviceState state) { return AggregateState(topologyState) == static_cast<AggregatedState>(state); }

inline TopologyStateByCollection GroupByCollectionId(const TopologyState& topoState)
{
    TopologyStateByCollection state;
    for (const auto& ds : topoState) {
        if (ds.collectionId != 0) {
            state[ds.collectionId].push_back(ds);
        }
    }

    return state;
}

inline TopologyStateByTask GroupByTaskId(const TopologyState& topoState)
{
    TopologyStateByTask state;
    for (const auto& ds : topoState) {
        state[ds.taskId] = ds;
    }

    return state;
}

using Duration = std::chrono::microseconds;

} // namespace odc::core

#endif /* ODC_TOPOLOGYDEFS */
