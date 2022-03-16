/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__Topology__
#define __ODC__Topology__

#include <odc/AsioAsyncOp.h>
#include <odc/AsioBase.h>
#include <odc/Error.h>
#include <odc/MiscUtils.h>
#include <odc/Semaphore.h>
#include <odc/cc/CustomCommands.h>

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/system_executor.hpp>

#include <dds/Tools.h>
#include <dds/Topology.h>

#include <fairmq/States.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
    Undefined = static_cast<int>(fair::mq::State::Undefined),
    Ok = static_cast<int>(fair::mq::State::Ok),
    Error = static_cast<int>(fair::mq::State::Error),
    Idle = static_cast<int>(fair::mq::State::Idle),
    InitializingDevice = static_cast<int>(fair::mq::State::InitializingDevice),
    Initialized = static_cast<int>(fair::mq::State::Initialized),
    Binding = static_cast<int>(fair::mq::State::Binding),
    Bound = static_cast<int>(fair::mq::State::Bound),
    Connecting = static_cast<int>(fair::mq::State::Connecting),
    DeviceReady = static_cast<int>(fair::mq::State::DeviceReady),
    InitializingTask = static_cast<int>(fair::mq::State::InitializingTask),
    Ready = static_cast<int>(fair::mq::State::Ready),
    Running = static_cast<int>(fair::mq::State::Running),
    ResettingTask = static_cast<int>(fair::mq::State::ResettingTask),
    ResettingDevice = static_cast<int>(fair::mq::State::ResettingDevice),
    Exiting = static_cast<int>(fair::mq::State::Exiting),
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
using DevicePropertyQuery = std::string; /// Boost regex supported
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

inline AggregatedState AggregateState(const TopologyState& topologyState)
{
    DeviceState first = topologyState.begin()->state;

    if (std::all_of(topologyState.cbegin(), topologyState.cend(), [&](TopologyState::value_type i) { return i.state == first; })) {
        return static_cast<AggregatedState>(first);
    }

    return AggregatedState::Mixed;
}

inline bool StateEqualsTo(const TopologyState& topologyState, DeviceState state) { return AggregateState(topologyState) == static_cast<AggregatedState>(state); }

inline TopologyStateByCollection GroupByCollectionId(const TopologyState& topologyState)
{
    TopologyStateByCollection state;
    for (const auto& ds : topologyState) {
        if (ds.collectionId != 0) {
            state[ds.collectionId].push_back(ds);
        }
    }

    return state;
}

inline TopologyStateByTask GroupByTaskId(const TopologyState& topologyState)
{
    TopologyStateByTask state;
    for (const auto& ds : topologyState) {
        state[ds.taskId] = ds;
    }

    return state;
}

/**
 * @class BasicTopology Topology.h <fairmq/sdk/Topology.h>
 * @tparam Executor Associated I/O executor
 * @tparam Allocator Associated default allocator
 * @brief Represents a FairMQ topology
 *
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Safe.
 */
template<typename Executor, typename Allocator>
class BasicTopology : public AsioBase<Executor, Allocator>
{
  public:
    /// @brief (Re)Construct a FairMQ topology from an existing DDS topology
    /// @param topo CTopology
    /// @param session CSession
    /// @param blockUntilConnected if true, ctor will wait for all tasks to confirm subscriptions
    BasicTopology(dds::topology_api::CTopology topo, std::shared_ptr<dds::tools_api::CSession> session, bool blockUntilConnected = false)
        : BasicTopology<Executor, Allocator>(boost::asio::system_executor(), std::move(topo), std::move(session), blockUntilConnected)
    {
    }

    /// @brief (Re)Construct a FairMQ topology from an existing DDS topology
    /// @param ex I/O executor to be associated
    /// @param topo CTopology
    /// @param session CSession
    /// @param blockUntilConnected if true, ctor will wait for all tasks to confirm subscriptions
    /// @throws RuntimeError
    BasicTopology(const Executor& ex,
                  dds::topology_api::CTopology topo,
                  std::shared_ptr<dds::tools_api::CSession> session,
                  bool blockUntilConnected = false,
                  Allocator alloc = DefaultAllocator())
        : AsioBase<Executor, Allocator>(ex, std::move(alloc))
        , fDDSSession(session)
        , fDDSCustomCmd(fDDSService)
        , fDDSTopo(topo)
        , fMtx(std::make_unique<std::mutex>())
        , fStateChangeSubscriptionsCV(std::make_unique<std::condition_variable>())
        , fNumStateChangePublishers(0)
        , fHeartbeatsTimer(boost::asio::system_executor())
        , fHeartbeatInterval(600000)
    {
        makeTopologyState();

        // We assume from here the given CSession has an active topo that matches the given topo file!
        // std::string activeTopo(fDDSSession.RequestCommanderInfo().activeTopologyName);
        // std::string givenTopo(fDDSTopo.GetName());
        // if (activeTopo != givenTopo) {
        // throw RuntimeError("Given topology ", givenTopo, " is not activated (active: ", activeTopo, ")");
        // }

        SubscribeToCommands();
        SubscribeToTaskDoneEvents();

        fDDSService.start(to_string(fDDSSession->getSessionID()));
        SubscribeToStateChanges();
        if (blockUntilConnected) {
            WaitForPublisherCount(fStateIndex.size());
        }
    }

    /// not copyable
    BasicTopology(const BasicTopology&) = delete;
    BasicTopology& operator=(const BasicTopology&) = delete;

    /// movable
    BasicTopology(BasicTopology&&) = default;
    BasicTopology& operator=(BasicTopology&&) = default;

    ~BasicTopology()
    {
        UnsubscribeFromStateChanges();

        std::lock_guard<std::mutex> lk(*fMtx);
        fDDSCustomCmd.unsubscribe();
        try {
            for (auto& op : fChangeStateOps) {
                op.second.Complete(MakeErrorCode(ErrorCode::OperationCanceled));
            }
        } catch (...) {
        }
        fDDSOnTaskDoneRequest->unsubscribeResponseCallback();
    }

    auto GetTasks(const std::string& path = "") const -> std::vector<DDSTask>
    {
        std::vector<DDSTask> list;

        dds::topology_api::STopoRuntimeTask::FilterIteratorPair_t itPair;
        if (path.empty()) {
            itPair = fDDSTopo.getRuntimeTaskIterator(nullptr); // passing nullptr will get all tasks
        } else {
            itPair = fDDSTopo.getRuntimeTaskIteratorMatchingPath(path);
        }
        auto tasks = boost::make_iterator_range(itPair.first, itPair.second);

        list.reserve(boost::size(tasks));

        for (const auto& task : tasks) {
            // LOG(debug) << "Found task with id: " << task.first << ", "
            //            << "Path: " << task.second.m_taskPath << ", "
            //            << "Collection id: " << task.second.m_taskCollectionId << ", "
            //            << "Name: " << task.second.m_task->getName() << "_" << task.second.m_taskIndex;
            list.emplace_back(task.first, task.second.m_taskCollectionId);
        }

        return list;
    }

    void SubscribeToStateChanges()
    {
        // FAIR_LOG(debug) << "Subscribing to state change";
        cc::Cmds cmds(cc::make<cc::SubscribeToStateChange>(fHeartbeatInterval.count()));
        fDDSCustomCmd.send(cmds.Serialize(), "");

        fHeartbeatsTimer.expires_after(fHeartbeatInterval);
        fHeartbeatsTimer.async_wait(std::bind(&BasicTopology::SendSubscriptionHeartbeats, this, std::placeholders::_1));
    }

    void SubscribeToTaskDoneEvents()
    {
        using namespace dds::tools_api;
        SOnTaskDoneRequest::request_t request;
        fDDSOnTaskDoneRequest = SOnTaskDoneRequest::makeRequest(request);
        fDDSOnTaskDoneRequest->setResponseCallback([&](const SOnTaskDoneResponseData& info) {
            std::unique_lock<std::mutex> lk(*fMtx);
            DeviceStatus& task = fStateData.at(fStateIndex.at(info.m_taskID));
            if (task.subscribedToStateChanges) {
                task.subscribedToStateChanges = false;
                --fNumStateChangePublishers;
            }
            task.exitCode = info.m_exitCode;
            task.signal = info.m_signal;
            task.lastState = task.state;
            task.state = DeviceState::Error;

            if (task.exitCode > 0) {
                for (auto& op : fChangeStateOps) {
                    op.second.Update(task.taskId, DeviceState::Error);
                }
                for (auto& op : fWaitForStateOps) {
                    op.second.Update(task.taskId, DeviceState::Error, DeviceState::Error);
                }
                // TODO: include set/get property ops
            }
        });
        fDDSSession->sendRequest<SOnTaskDoneRequest>(fDDSOnTaskDoneRequest);
    }

    void WaitForPublisherCount(unsigned int number)
    {
        using namespace std::chrono_literals;
        std::unique_lock<std::mutex> lk(*fMtx);
        auto publisherCountReached = [&]() { return fNumStateChangePublishers == number; };
        auto count(0);
        constexpr auto checkInterval(50ms);
        constexpr auto maxCount(30s / checkInterval);
        while (!publisherCountReached() && fDDSSession->IsRunning() && count < maxCount) {
            fStateChangeSubscriptionsCV->wait_for(lk, checkInterval, publisherCountReached);
            ++count;
        }
    }

    void SendSubscriptionHeartbeats(const boost::system::error_code& ec)
    {
        if (!ec) {
            // Timer expired.
            fDDSCustomCmd.send(cc::Cmds(cc::make<cc::SubscriptionHeartbeat>(fHeartbeatInterval.count())).Serialize(), "");
            // schedule again
            fHeartbeatsTimer.expires_after(fHeartbeatInterval);
            fHeartbeatsTimer.async_wait(std::bind(&BasicTopology::SendSubscriptionHeartbeats, this, std::placeholders::_1));
        } else if (ec == boost::asio::error::operation_aborted) {
            // OLOG(debug) << "Heartbeats timer canceled";
        } else {
            OLOG(error) << "Timer error: " << ec;
        }
    }

    void UnsubscribeFromStateChanges()
    {
        // stop sending heartbeats
        fHeartbeatsTimer.cancel();

        // unsubscribe from state changes
        fDDSCustomCmd.send(cc::Cmds(cc::make<cc::UnsubscribeFromStateChange>()).Serialize(), "");

        // wait for all tasks to confirm unsubscription
        WaitForPublisherCount(0);
    }

    void SubscribeToCommands()
    {
        fDDSCustomCmd.subscribe([&](const std::string& msg, const std::string& /* condition */, uint64_t ddsSenderChannelId) {
            cc::Cmds inCmds;
            inCmds.Deserialize(msg);
            // OLOG(debug) << "Received " << inCmds.Size() << " command(s) with total size of " <<
            // msg.length() << " bytes: ";

            for (const auto& cmd : inCmds) {
                // OLOG(debug) << " > " << cmd->GetType();
                switch (cmd->GetType()) {
                    case cc::Type::state_change_subscription:
                        HandleCmd(static_cast<cc::StateChangeSubscription&>(*cmd));
                        break;
                    case cc::Type::state_change_unsubscription:
                        HandleCmd(static_cast<cc::StateChangeUnsubscription&>(*cmd));
                        break;
                    case cc::Type::state_change:
                        HandleCmd(static_cast<cc::StateChange&>(*cmd), ddsSenderChannelId);
                        break;
                    case cc::Type::transition_status:
                        HandleCmd(static_cast<cc::TransitionStatus&>(*cmd));
                        break;
                    case cc::Type::properties:
                        HandleCmd(static_cast<cc::Properties&>(*cmd));
                        break;
                    case cc::Type::properties_set:
                        HandleCmd(static_cast<cc::PropertiesSet&>(*cmd));
                        break;
                    default:
                        OLOG(warning) << "Unexpected/unknown command received: " << cmd->GetType();
                        OLOG(warning) << "Origin: " << ddsSenderChannelId;
                        break;
                }
            }
        });
    }

    auto HandleCmd(cc::StateChangeSubscription const& cmd) -> void
    {
        if (cmd.GetResult() == cc::Result::Ok) {
            DDSTask::Id taskId(cmd.GetTaskId());

            try {
                std::unique_lock<std::mutex> lk(*fMtx);
                DeviceStatus& task = fStateData.at(fStateIndex.at(taskId));
                if (!task.subscribedToStateChanges) {
                    task.subscribedToStateChanges = true;
                    ++fNumStateChangePublishers;
                } else {
                    OLOG(warning) << "Task '" << task.taskId << "' sent subscription confirmation more than once";
                }
                lk.unlock();
                fStateChangeSubscriptionsCV->notify_one();
            } catch (const std::exception& e) {
                OLOG(error) << "Exception in HandleCmd(cc::StateChangeSubscription const&): " << e.what();
                OLOG(error) << "Possibly no task with id '" << taskId << "'?";
            }
        } else {
            OLOG(error) << "State change subscription failed for device: " << cmd.GetDeviceId() << ", task id: " << cmd.GetTaskId();
        }
    }

    void HandleCmd(cc::StateChangeUnsubscription const& cmd)
    {
        if (cmd.GetResult() == cc::Result::Ok) {
            DDSTask::Id taskId(cmd.GetTaskId());

            try {
                std::unique_lock<std::mutex> lk(*fMtx);
                DeviceStatus& task = fStateData.at(fStateIndex.at(taskId));
                if (task.subscribedToStateChanges) {
                    task.subscribedToStateChanges = false;
                    --fNumStateChangePublishers;
                } else {
                    OLOG(warning) << "Task '" << task.taskId << "' sent unsubscription confirmation more than once";
                }
                lk.unlock();
                fStateChangeSubscriptionsCV->notify_one();
            } catch (const std::exception& e) {
                OLOG(error) << "Exception in HandleCmd(cc::StateChangeUnsubscription const&): " << e.what();
            }
        } else {
            OLOG(error) << "State change unsubscription failed for device: " << cmd.GetDeviceId() << ", task id: " << cmd.GetTaskId();
        }
    }

    auto HandleCmd(cc::StateChange const& cmd, uint64_t ddsSenderChannelId) -> void
    {
        if (cmd.GetCurrentState() == DeviceState::Exiting) {
            fDDSCustomCmd.send(cc::Cmds(cc::make<cc::StateChangeExitingReceived>()).Serialize(), std::to_string(ddsSenderChannelId));
        }

        DDSTask::Id taskId(cmd.GetTaskId());

        try {
            std::lock_guard<std::mutex> lk(*fMtx);
            DeviceStatus& task = fStateData.at(fStateIndex.at(taskId));
            task.lastState = cmd.GetLastState();
            task.state = cmd.GetCurrentState();
            // if the task is exiting, it will not respond to unsubscription request anymore, set it to false now.
            if (task.state == DeviceState::Exiting) {
                task.subscribedToStateChanges = false;
                --fNumStateChangePublishers;
            }
            // FAIR_LOG(debug) << "Updated state entry: taskId=" << taskId << ", state=" << state;

            for (auto& op : fChangeStateOps) {
                op.second.Update(taskId, cmd.GetCurrentState());
            }
            for (auto& op : fWaitForStateOps) {
                op.second.Update(taskId, cmd.GetLastState(), cmd.GetCurrentState());
            }
        } catch (const std::exception& e) {
            OLOG(error) << "Exception in HandleCmd(cmd::StateChange const&): " << e.what();
        }
    }

    auto HandleCmd(cc::TransitionStatus const& cmd) -> void
    {
        if (cmd.GetResult() != cc::Result::Ok) {
            DDSTask::Id taskId(cmd.GetTaskId());
            std::lock_guard<std::mutex> lk(*fMtx);
            for (auto& op : fChangeStateOps) {
                if (!op.second.IsCompleted() && op.second.ContainsTask(taskId)) {
                    if (fStateData.at(fStateIndex.at(taskId)).state != op.second.GetTargetState()) {
                        OLOG(error) << cmd.GetTransition() << " transition failed for " << cmd.GetDeviceId() << ", device is in " << cmd.GetCurrentState() << " state.";
                        op.second.Complete(MakeErrorCode(ErrorCode::DeviceChangeStateFailed));
                    } else {
                        OLOG(debug) << cmd.GetTransition() << " transition failed for " << cmd.GetDeviceId() << ", device is already in " << cmd.GetCurrentState() << " state.";
                    }
                }
            }
        }
    }

    auto HandleCmd(cc::Properties const& cmd) -> void
    {
        std::unique_lock<std::mutex> lk(*fMtx);
        try {
            auto& op(fGetPropertiesOps.at(cmd.GetRequestId()));
            lk.unlock();
            op.Update(cmd.GetTaskId(), cmd.GetResult(), cmd.GetProps());
        } catch (std::out_of_range& e) {
            OLOG(debug) << "GetProperties operation (request id: " << cmd.GetRequestId() << ") not found (probably completed or timed out), "
                        << "discarding reply of device " << cmd.GetDeviceId() << ", task id: " << cmd.GetTaskId();
        }
    }

    auto HandleCmd(cc::PropertiesSet const& cmd) -> void
    {
        std::unique_lock<std::mutex> lk(*fMtx);
        try {
            auto& op(fSetPropertiesOps.at(cmd.GetRequestId()));
            lk.unlock();
            op.Update(cmd.GetTaskId(), cmd.GetResult());
        } catch (std::out_of_range& e) {
            OLOG(debug) << "SetProperties operation (request id: " << cmd.GetRequestId() << ") not found (probably completed or timed out), "
                        << "discarding reply of device " << cmd.GetDeviceId() << ", task id: " << cmd.GetTaskId();
        }
    }

    using Duration = std::chrono::microseconds;
    using ChangeStateCompletionSignature = void(std::error_code, TopologyState);

  private:
    struct ChangeStateOp
    {
        using Id = std::size_t;
        using Count = unsigned int;

        template<typename Handler>
        ChangeStateOp(Id id,
                      const TopologyTransition transition,
                      std::vector<DDSTask> tasks,
                      TopologyState& stateData,
                      Duration timeout,
                      std::mutex& mutex,
                      Executor const& ex,
                      Allocator const& alloc,
                      Handler&& handler)
            : fId(id)
            , fOp(ex, alloc, std::move(handler))
            , fStateData(stateData)
            , fTimer(ex)
            , fCount(0)
            , fTasks(std::move(tasks))
            , fTargetState(gExpectedState.at(transition))
            , fMtx(mutex)
            , fErrored(false)
        {
            if (timeout > std::chrono::milliseconds(0)) {
                fTimer.expires_after(timeout);
                fTimer.async_wait([&](std::error_code ec) {
                    if (!ec) {
                        std::lock_guard<std::mutex> lk(fMtx);
                        fOp.Timeout(fStateData);
                    }
                });
            }
            if (fTasks.empty()) {
                OLOG(warning) << "ChangeState initiated on an empty set of tasks, check the path argument.";
            }
        }
        ChangeStateOp() = delete;
        ChangeStateOp(const ChangeStateOp&) = delete;
        ChangeStateOp& operator=(const ChangeStateOp&) = delete;
        ChangeStateOp(ChangeStateOp&&) = default;
        ChangeStateOp& operator=(ChangeStateOp&&) = default;
        ~ChangeStateOp() = default;

        /// precondition: fMtx is locked.
        auto ResetCount(const TopologyStateIndex& stateIndex, const TopologyState& stateData) -> void
        {
            fCount = std::count_if(stateIndex.cbegin(), stateIndex.cend(), [=](const auto& s) {
                if (ContainsTask(stateData.at(s.second).taskId)) {
                    return stateData.at(s.second).state == fTargetState;
                } else {
                    return false;
                }
            });
        }

        /// precondition: fMtx is locked.
        auto Update(const DDSTask::Id taskId, const DeviceState currentState) -> void
        {
            if (!fOp.IsCompleted() && ContainsTask(taskId)) {
                if (currentState == fTargetState) {
                    ++fCount;
                }
                if (currentState == DeviceState::Error) {
                    fErrored = true;
                    ++fCount;
                }
                TryCompletion();
            }
        }

        /// precondition: fMtx is locked.
        auto TryCompletion() -> void
        {
            if (!fOp.IsCompleted() && fCount == fTasks.size()) {
                if (fErrored) {
                    Complete(MakeErrorCode(ErrorCode::DeviceChangeStateFailed));
                } else {
                    Complete(std::error_code());
                }
            }
        }

        /// precondition: fMtx is locked.
        auto Complete(std::error_code ec) -> void
        {
            fTimer.cancel();
            fOp.Complete(ec, fStateData);
        }

        /// precondition: fMtx is locked.
        auto ContainsTask(DDSTask::Id id) -> bool
        {
            auto it = std::find_if(fTasks.begin(), fTasks.end(), [id](const DDSTask& t) { return t.GetId() == id; });
            return it != fTasks.end();
        }

        bool IsCompleted() { return fOp.IsCompleted(); }

        auto GetTargetState() const -> DeviceState { return fTargetState; }

      private:
        Id const fId;
        AsioAsyncOp<Executor, Allocator, ChangeStateCompletionSignature> fOp;
        TopologyState& fStateData;
        boost::asio::steady_timer fTimer;
        Count fCount;
        std::vector<DDSTask> fTasks;
        DeviceState fTargetState;
        std::mutex& fMtx;
        bool fErrored;
    };

  public:
    /// @brief Initiate state transition on all FairMQ devices in this topology
    /// @param transition FairMQ device state machine transition
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    ///
    /// @par Usage examples
    /// With lambda:
    /// @code
    /// topo.AsyncChangeState(
    ///     odc::core::TopologyTransition::InitDevice,
    ///     std::chrono::milliseconds(500),
    ///     [](std::error_code ec, odc::core::TopologyState state) {
    ///         if (!ec) {
    ///             // success
    ///          } else if (ec.category().name() == "fairmq") {
    ///             switch (static_cast<odc::ErrorCode>(ec.value())) {
    ///               case odc::core::ErrorCode::OperationTimeout:
    ///                 // async operation timed out
    ///               case odc::core::ErrorCode::OperationCanceled:
    ///                 // async operation canceled
    ///               case odc::core::ErrorCode::DeviceChangeStateFailed:
    ///                 // failed to change state of a fairmq device
    ///               default:
    ///             }
    ///         }
    ///     }
    /// );
    /// @endcode
    /// With future:
    /// @code
    /// auto fut = topo.AsyncChangeState(odc::core::TopologyTransition::InitDevice,
    ///                                  std::chrono::milliseconds(500),
    ///                                  boost::asio::use_future);
    /// try {
    ///     odc::core::TopologyState state = fut.get();
    ///     // success
    /// } catch (const std::system_error& ex) {
    ///     auto ec(ex.code());
    ///     if (ec.category().name() == "odc") {
    ///         switch (static_cast<odc::core::ErrorCode>(ec.value())) {
    ///           case odc::core::ErrorCode::OperationTimeout:
    ///             // async operation timed out
    ///           case odc::core::ErrorCode::OperationCanceled:
    ///             // async operation canceled
    ///           case odc::core::ErrorCode::DeviceChangeStateFailed:
    ///             // failed to change state of a fairmq device
    ///           default:
    ///         }
    ///     }
    /// }
    /// @endcode
    /// With coroutine (C++20, see https://en.cppreference.com/w/cpp/language/coroutines):
    /// @code
    /// try {
    ///     odc::core::TopologyState state = co_await
    ///         topo.AsyncChangeState(odc::core::TopologyTransition::InitDevice,
    ///                               std::chrono::milliseconds(500),
    ///                               boost::asio::use_awaitable);
    ///     // success
    /// } catch (const std::system_error& ex) {
    ///     auto ec(ex.code());
    ///     if (ec.category().name() == "odc") {
    ///         switch (static_cast<odc::core::ErrorCode>(ec.value())) {
    ///           case odc::core::ErrorCode::OperationTimeout:
    ///             // async operation timed out
    ///           case odc::core::ErrorCode::OperationCanceled:
    ///             // async operation canceled
    ///           case odc::core::ErrorCode::DeviceChangeStateFailed:
    ///             // failed to change state of a fairmq device
    ///           default:
    ///         }
    ///     }
    /// }
    /// @endcode
    template<typename CompletionToken>
    auto AsyncChangeState(const TopologyTransition transition, const std::string& path, Duration timeout, CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, ChangeStateCompletionSignature>(
            [&](auto handler) {
                typename ChangeStateOp::Id const id(uuidHash());

                std::lock_guard<std::mutex> lk(*fMtx);

                for (auto it = begin(fChangeStateOps); it != end(fChangeStateOps);) {
                    if (it->second.IsCompleted()) {
                        it = fChangeStateOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                auto p = fChangeStateOps.emplace(std::piecewise_construct,
                                                 std::forward_as_tuple(id),
                                                 std::forward_as_tuple(id,
                                                                       transition,
                                                                       GetTasks(path),
                                                                       fStateData,
                                                                       timeout,
                                                                       *fMtx,
                                                                       AsioBase<Executor, Allocator>::GetExecutor(),
                                                                       AsioBase<Executor, Allocator>::GetAllocator(),
                                                                       std::move(handler)));

                cc::Cmds cmds(cc::make<cc::ChangeState>(transition));
                fDDSCustomCmd.send(cmds.Serialize(), path);

                p.first->second.ResetCount(fStateIndex, fStateData);
                // TODO: make sure following operation properly queues the completion and not doing it directly out
                // of initiation call.
                p.first->second.TryCompletion();
            },
            token);
    }

    /// @brief Initiate state transition on all FairMQ devices in this topology
    /// @param transition FairMQ device state machine transition
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncChangeState(const TopologyTransition transition, CompletionToken&& token)
    {
        return AsyncChangeState(transition, "", Duration(0), std::move(token));
    }

    /// @brief Initiate state transition on all FairMQ devices in this topology with a timeout
    /// @param transition FairMQ device state machine transition
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncChangeState(const TopologyTransition transition, Duration timeout, CompletionToken&& token)
    {
        return AsyncChangeState(transition, "", timeout, std::move(token));
    }

    /// @brief Initiate state transition on all FairMQ devices in this topology with a timeout
    /// @param transition FairMQ device state machine transition
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncChangeState(const TopologyTransition transition, const std::string& path, CompletionToken&& token)
    {
        return AsyncChangeState(transition, path, Duration(0), std::move(token));
    }

    /// @brief Perform state transition on FairMQ devices in this topology for a specified topology path
    /// @param transition FairMQ device state machine transition
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    auto ChangeState(const TopologyTransition transition, const std::string& path = "", Duration timeout = Duration(0)) -> std::pair<std::error_code, TopologyState>
    {
        SharedSemaphore blocker;
        std::error_code ec;
        TopologyState state;
        AsyncChangeState(transition, path, timeout, [&, blocker](std::error_code _ec, TopologyState _state) mutable {
            ec = _ec;
            state = _state;
            blocker.Signal();
        });
        blocker.Wait();
        return { ec, state };
    }

    /// @brief Perform state transition on all FairMQ devices in this topology with a timeout
    /// @param transition FairMQ device state machine transition
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    auto ChangeState(const TopologyTransition transition, Duration timeout) -> std::pair<std::error_code, TopologyState> { return ChangeState(transition, "", timeout); }

    /// @brief Returns the current state of the topology
    /// @return map of id : DeviceStatus
    auto GetCurrentState() const -> TopologyState
    {
        std::lock_guard<std::mutex> lk(*fMtx);
        return fStateData;
    }

    auto AggregateState() const -> DeviceState { return AggregateState(GetCurrentState()); }

    auto StateEqualsTo(DeviceState state) const -> bool { return StateEqualsTo(GetCurrentState(), state); }

    using WaitForStateCompletionSignature = void(std::error_code);

  private:
    struct WaitForStateOp
    {
        using Id = std::size_t;
        using Count = unsigned int;

        template<typename Handler>
        WaitForStateOp(Id id,
                       DeviceState targetLastState,
                       DeviceState targetCurrentState,
                       std::vector<DDSTask> tasks,
                       Duration timeout,
                       std::mutex& mutex,
                       Executor const& ex,
                       Allocator const& alloc,
                       Handler&& handler)
            : fId(id)
            , fOp(ex, alloc, std::move(handler))
            , fTimer(ex)
            , fCount(0)
            , fTasks(std::move(tasks))
            , fTargetLastState(targetLastState)
            , fTargetCurrentState(targetCurrentState)
            , fMtx(mutex)
            , fErrored(false)
        {
            if (timeout > std::chrono::milliseconds(0)) {
                fTimer.expires_after(timeout);
                fTimer.async_wait([&](std::error_code ec) {
                    if (!ec) {
                        std::lock_guard<std::mutex> lk(fMtx);
                        fOp.Timeout();
                    }
                });
            }
            if (fTasks.empty()) {
                OLOG(warning) << "WaitForState initiated on an empty set of tasks, check the path argument.";
            }
        }
        WaitForStateOp() = delete;
        WaitForStateOp(const WaitForStateOp&) = delete;
        WaitForStateOp& operator=(const WaitForStateOp&) = delete;
        WaitForStateOp(WaitForStateOp&&) = default;
        WaitForStateOp& operator=(WaitForStateOp&&) = default;
        ~WaitForStateOp() = default;

        /// precondition: fMtx is locked.
        auto ResetCount(const TopologyStateIndex& stateIndex, const TopologyState& stateData) -> void
        {
            fCount = std::count_if(stateIndex.cbegin(), stateIndex.cend(), [=](const auto& s) {
                if (ContainsTask(stateData.at(s.second).taskId)) {
                    return stateData.at(s.second).state == fTargetCurrentState && (stateData.at(s.second).lastState == fTargetLastState || fTargetLastState == DeviceState::Undefined);
                } else {
                    return false;
                }
            });
        }

        /// precondition: fMtx is locked.
        auto Update(const DDSTask::Id taskId, const DeviceState lastState, const DeviceState currentState) -> void
        {
            if (!fOp.IsCompleted() && ContainsTask(taskId)) {
                if (currentState == fTargetCurrentState && (lastState == fTargetLastState || fTargetLastState == DeviceState::Undefined)) {
                    ++fCount;
                }
                if (currentState == DeviceState::Error) {
                    fErrored = true;
                    ++fCount;
                }
                TryCompletion();
            }
        }

        /// precondition: fMtx is locked.
        auto TryCompletion() -> void
        {
            if (!fOp.IsCompleted() && fCount == fTasks.size()) {
                fTimer.cancel();
                if (fErrored) {
                    fOp.Complete(MakeErrorCode(ErrorCode::DeviceChangeStateFailed));
                } else {
                    fOp.Complete();
                }
            }
        }

        bool IsCompleted() { return fOp.IsCompleted(); }

      private:
        Id const fId;
        AsioAsyncOp<Executor, Allocator, WaitForStateCompletionSignature> fOp;
        boost::asio::steady_timer fTimer;
        Count fCount;
        std::vector<DDSTask> fTasks;
        DeviceState fTargetLastState;
        DeviceState fTargetCurrentState;
        std::mutex& fMtx;
        bool fErrored;

        /// precondition: fMtx is locked.
        auto ContainsTask(DDSTask::Id id) -> bool
        {
            auto it = std::find_if(fTasks.begin(), fTasks.end(), [id](const DDSTask& t) { return t.GetId() == id; });
            return it != fTasks.end();
        }
    };

  public:
    /// @brief Initiate waiting for selected FairMQ devices to reach given last & current state in this topology
    /// @param targetLastState the target last device state to wait for
    /// @param targetCurrentState the target device state to wait for
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncWaitForState(const DeviceState targetLastState, const DeviceState targetCurrentState, const std::string& path, Duration timeout, CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, WaitForStateCompletionSignature>(
            [&](auto handler) {
                typename GetPropertiesOp::Id const id(uuidHash());

                std::lock_guard<std::mutex> lk(*fMtx);

                for (auto it = begin(fWaitForStateOps); it != end(fWaitForStateOps);) {
                    if (it->second.IsCompleted()) {
                        it = fWaitForStateOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                auto p = fWaitForStateOps.emplace(std::piecewise_construct,
                                                  std::forward_as_tuple(id),
                                                  std::forward_as_tuple(id,
                                                                        targetLastState,
                                                                        targetCurrentState,
                                                                        GetTasks(path),
                                                                        timeout,
                                                                        *fMtx,
                                                                        AsioBase<Executor, Allocator>::GetExecutor(),
                                                                        AsioBase<Executor, Allocator>::GetAllocator(),
                                                                        std::move(handler)));
                p.first->second.ResetCount(fStateIndex, fStateData);
                // TODO: make sure following operation properly queues the completion and not doing it directly out
                // of initiation call.
                p.first->second.TryCompletion();
            },
            token);
    }

    /// @brief Initiate waiting for selected FairMQ devices to reach given last & current state in this topology
    /// @param targetLastState the target last device state to wait for
    /// @param targetCurrentState the target device state to wait for
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncWaitForState(const DeviceState targetLastState, const DeviceState targetCurrentState, CompletionToken&& token)
    {
        return AsyncWaitForState(targetLastState, targetCurrentState, "", Duration(0), std::move(token));
    }

    /// @brief Initiate waiting for selected FairMQ devices to reach given current state in this topology
    /// @param targetCurrentState the target device state to wait for
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncWaitForState(const DeviceState targetCurrentState, CompletionToken&& token)
    {
        return AsyncWaitForState(DeviceState::Undefined, targetCurrentState, "", Duration(0), std::move(token));
    }

    /// @brief Wait for selected FairMQ devices to reach given last & current state in this topology
    /// @param targetLastState the target last device state to wait for
    /// @param targetCurrentState the target device state to wait for
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    auto WaitForState(const DeviceState targetLastState, const DeviceState targetCurrentState, const std::string& path = "", Duration timeout = Duration(0)) -> std::error_code
    {
        SharedSemaphore blocker;
        std::error_code ec;
        AsyncWaitForState(targetLastState, targetCurrentState, path, timeout, [&, blocker](std::error_code _ec) mutable {
            ec = _ec;
            blocker.Signal();
        });
        blocker.Wait();
        return ec;
    }

    /// @brief Wait for selected FairMQ devices to reach given current state in this topology
    /// @param targetCurrentState the target device state to wait for
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    auto WaitForState(const DeviceState targetCurrentState, const std::string& path = "", Duration timeout = Duration(0)) -> std::error_code
    {
        return WaitForState(DeviceState::Undefined, targetCurrentState, path, timeout);
    }

    using GetPropertiesCompletionSignature = void(std::error_code, GetPropertiesResult);

  private:
    struct GetPropertiesOp
    {
        using Id = std::size_t;
        using GetCount = unsigned int;

        template<typename Handler>
        GetPropertiesOp(Id id, std::vector<DDSTask> tasks, Duration timeout, std::mutex& mutex, Executor const& ex, Allocator const& alloc, Handler&& handler)
            : fId(id)
            , fOp(ex, alloc, std::move(handler))
            , fTimer(ex)
            , fCount(0)
            , fTasks(std::move(tasks))
            , fMtx(mutex)
        {
            if (timeout > std::chrono::milliseconds(0)) {
                fTimer.expires_after(timeout);
                fTimer.async_wait([&](std::error_code ec) {
                    if (!ec) {
                        std::lock_guard<std::mutex> lk(fMtx);
                        fOp.Timeout(fResult);
                    }
                });
            }
            if (fTasks.empty()) {
                OLOG(warning) << "GetProperties initiated on an empty set of tasks, check the path argument.";
            }

            fResult.failed.reserve(fTasks.size());
            for (const auto& task : fTasks) {
                fResult.failed.emplace(task.GetId());
            }

            // OLOG(debug) << "GetProperties " << fId << " with expected count of " << fTasks.size() <<
            // " started.";
        }
        GetPropertiesOp() = delete;
        GetPropertiesOp(const GetPropertiesOp&) = delete;
        GetPropertiesOp& operator=(const GetPropertiesOp&) = delete;
        GetPropertiesOp(GetPropertiesOp&&) = default;
        GetPropertiesOp& operator=(GetPropertiesOp&&) = default;
        ~GetPropertiesOp() = default;

        auto Update(const DDSTask::Id taskId, cc::Result result, DeviceProperties props) -> void
        {
            std::lock_guard<std::mutex> lk(fMtx);
            if (result == cc::Result::Ok) {
                fResult.failed.erase(taskId);
                fResult.devices.insert({ taskId, { std::move(props) } });
            }
            ++fCount;
            TryCompletion();
        }

        bool IsCompleted() { return fOp.IsCompleted(); }

      private:
        Id const fId;
        AsioAsyncOp<Executor, Allocator, GetPropertiesCompletionSignature> fOp;
        boost::asio::steady_timer fTimer;
        GetCount fCount;
        std::vector<DDSTask> fTasks;
        GetPropertiesResult fResult;
        std::mutex& fMtx;

        /// precondition: fMtx is locked.
        auto TryCompletion() -> void
        {
            if (!fOp.IsCompleted() && fCount == fTasks.size()) {
                fTimer.cancel();
                if (!fResult.failed.empty()) {
                    fOp.Complete(MakeErrorCode(ErrorCode::DeviceGetPropertiesFailed), std::move(fResult));
                } else {
                    fOp.Complete(std::move(fResult));
                }
            }
        }
    };

  public:
    /// @brief Initiate property query on selected FairMQ devices in this topology
    /// @param query Key(s) to be queried (regex)
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncGetProperties(DevicePropertyQuery const& query, const std::string& path, Duration timeout, CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, GetPropertiesCompletionSignature>(
            [&](auto handler) {
                typename GetPropertiesOp::Id const id(uuidHash());

                std::lock_guard<std::mutex> lk(*fMtx);

                for (auto it = begin(fGetPropertiesOps); it != end(fGetPropertiesOps);) {
                    if (it->second.IsCompleted()) {
                        it = fGetPropertiesOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                fGetPropertiesOps.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(id),
                    std::forward_as_tuple(id, GetTasks(path), timeout, *fMtx, AsioBase<Executor, Allocator>::GetExecutor(), AsioBase<Executor, Allocator>::GetAllocator(), std::move(handler)));

                cc::Cmds const cmds(cc::make<cc::GetProperties>(id, query));
                fDDSCustomCmd.send(cmds.Serialize(), path);
            },
            token);
    }

    /// @brief Initiate property query on selected FairMQ devices in this topology
    /// @param query Key(s) to be queried (regex)
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncGetProperties(DevicePropertyQuery const& query, CompletionToken&& token)
    {
        return AsyncGetProperties(query, "", Duration(0), std::move(token));
    }

    /// @brief Query properties on selected FairMQ devices in this topology
    /// @param query Key(s) to be queried (regex)
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    auto GetProperties(DevicePropertyQuery const& query, const std::string& path = "", Duration timeout = Duration(0)) -> std::pair<std::error_code, GetPropertiesResult>
    {
        SharedSemaphore blocker;
        std::error_code ec;
        GetPropertiesResult result;
        AsyncGetProperties(query, path, timeout, [&, blocker](std::error_code _ec, GetPropertiesResult _result) mutable {
            ec = _ec;
            result = _result;
            blocker.Signal();
        });
        blocker.Wait();
        return { ec, result };
    }

    using SetPropertiesCompletionSignature = void(std::error_code, FailedDevices);

  private:
    struct SetPropertiesOp
    {
        using Id = std::size_t;
        using SetCount = unsigned int;

        template<typename Handler>
        SetPropertiesOp(Id id, std::vector<DDSTask> tasks, Duration timeout, std::mutex& mutex, Executor const& ex, Allocator const& alloc, Handler&& handler)
            : fId(id)
            , fOp(ex, alloc, std::move(handler))
            , fTimer(ex)
            , fCount(0)
            , fTasks(std::move(tasks))
            , fMtx(mutex)
        {
            if (timeout > std::chrono::milliseconds(0)) {
                fTimer.expires_after(timeout);
                fTimer.async_wait([&](std::error_code ec) {
                    if (!ec) {
                        std::lock_guard<std::mutex> lk(fMtx);
                        fOp.Timeout(fFailedDevices);
                    }
                });
            }
            if (fTasks.empty()) {
                OLOG(warning) << "SetProperties initiated on an empty set of tasks, check the path argument.";
            }

            fFailedDevices.reserve(fTasks.size());
            for (const auto& task : fTasks) {
                fFailedDevices.emplace(task.GetId());
            }

            // OLOG(debug) << "SetProperties " << fId << " with expected count of " << fTasks.size() << " started.";
        }
        SetPropertiesOp() = delete;
        SetPropertiesOp(const SetPropertiesOp&) = delete;
        SetPropertiesOp& operator=(const SetPropertiesOp&) = delete;
        SetPropertiesOp(SetPropertiesOp&&) = default;
        SetPropertiesOp& operator=(SetPropertiesOp&&) = default;
        ~SetPropertiesOp() = default;

        auto Update(const DDSTask::Id taskId, cc::Result result) -> void
        {
            std::lock_guard<std::mutex> lk(fMtx);
            if (result == cc::Result::Ok) {
                fFailedDevices.erase(taskId);
            }
            ++fCount;
            TryCompletion();
        }

        bool IsCompleted() { return fOp.IsCompleted(); }

      private:
        Id const fId;
        AsioAsyncOp<Executor, Allocator, SetPropertiesCompletionSignature> fOp;
        boost::asio::steady_timer fTimer;
        SetCount fCount;
        std::vector<DDSTask> fTasks;
        FailedDevices fFailedDevices;
        std::mutex& fMtx;

        /// precondition: fMtx is locked.
        auto TryCompletion() -> void
        {
            if (!fOp.IsCompleted() && fCount == fTasks.size()) {
                fTimer.cancel();
                if (!fFailedDevices.empty()) {
                    fOp.Complete(MakeErrorCode(ErrorCode::DeviceSetPropertiesFailed), fFailedDevices);
                } else {
                    fOp.Complete(fFailedDevices);
                }
            }
        }
    };

  public:
    /// @brief Initiate property update on selected FairMQ devices in this topology
    /// @param props Properties to set
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncSetProperties(const DeviceProperties& props, const std::string& path, Duration timeout, CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, SetPropertiesCompletionSignature>(
            [&](auto handler) {
                typename SetPropertiesOp::Id const id(uuidHash());

                std::lock_guard<std::mutex> lk(*fMtx);

                for (auto it = begin(fGetPropertiesOps); it != end(fGetPropertiesOps);) {
                    if (it->second.IsCompleted()) {
                        it = fGetPropertiesOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                fSetPropertiesOps.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(id),
                    std::forward_as_tuple(id, GetTasks(path), timeout, *fMtx, AsioBase<Executor, Allocator>::GetExecutor(), AsioBase<Executor, Allocator>::GetAllocator(), std::move(handler)));

                cc::Cmds const cmds(cc::make<cc::SetProperties>(id, props));
                fDDSCustomCmd.send(cmds.Serialize(), path);
            },
            token);
    }

    /// @brief Initiate property update on selected FairMQ devices in this topology
    /// @param props Properties to set
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncSetProperties(DeviceProperties const& props, CompletionToken&& token)
    {
        return AsyncSetProperties(props, "", Duration(0), std::move(token));
    }

    /// @brief Set properties on selected FairMQ devices in this topology
    /// @param props Properties to set
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    auto SetProperties(DeviceProperties const& properties, const std::string& path = "", Duration timeout = Duration(0)) -> std::pair<std::error_code, FailedDevices>
    {
        SharedSemaphore blocker;
        std::error_code ec;
        FailedDevices failed;
        AsyncSetProperties(properties, path, timeout, [&, blocker](std::error_code _ec, FailedDevices _failed) mutable {
            ec = _ec;
            failed = _failed;
            blocker.Signal();
        });
        blocker.Wait();
        return { ec, failed };
    }

    Duration GetHeartbeatInterval() const { return fHeartbeatInterval; }
    void SetHeartbeatInterval(Duration duration) { fHeartbeatInterval = duration; }

  private:
    using TransitionedCount = unsigned int;

    std::shared_ptr<dds::tools_api::CSession> fDDSSession;
    dds::intercom_api::CIntercomService fDDSService;
    dds::intercom_api::CCustomCmd fDDSCustomCmd;
    dds::topology_api::CTopology fDDSTopo;
    dds::tools_api::SOnTaskDoneRequest::ptr_t fDDSOnTaskDoneRequest;
    TopologyState fStateData;
    TopologyStateIndex fStateIndex;

    mutable std::unique_ptr<std::mutex> fMtx;

    std::unique_ptr<std::condition_variable> fStateChangeSubscriptionsCV;
    unsigned int fNumStateChangePublishers;
    boost::asio::steady_timer fHeartbeatsTimer;
    Duration fHeartbeatInterval;

    std::unordered_map<typename ChangeStateOp::Id, ChangeStateOp> fChangeStateOps;
    std::unordered_map<typename WaitForStateOp::Id, WaitForStateOp> fWaitForStateOps;
    std::unordered_map<typename SetPropertiesOp::Id, SetPropertiesOp> fSetPropertiesOps;
    std::unordered_map<typename GetPropertiesOp::Id, GetPropertiesOp> fGetPropertiesOps;

    auto makeTopologyState() -> void
    {
        fStateData.reserve(GetTasks().size());

        int index = 0;

        for (const auto& task : GetTasks()) {
            fStateData.push_back(DeviceStatus{ false, DeviceState::Undefined, DeviceState::Undefined, task.GetId(), task.GetCollectionId(), -1, -1 });
            fStateIndex.emplace(task.GetId(), index);
            index++;
        }
    }

    /// precodition: fMtx is locked.
    auto GetCurrentStateUnsafe() const -> TopologyState { return fStateData; }
};

using Topology = BasicTopology<DefaultExecutor, DefaultAllocator>;
using Topo = Topology;

} // namespace odc::core

#endif /* __ODC__Topology__ */
