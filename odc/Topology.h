/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_TOPOLOGY
#define ODC_TOPOLOGY

#include <odc/AsioAsyncOp.h>
#include <odc/AsioBase.h>
#include <odc/Error.h>
#include <odc/MiscUtils.h>
#include <odc/Semaphore.h>
#include <odc/TopologyDefs.h>
#include <odc/TopologyOpChangeState.h>
#include <odc/TopologyOpGetProperties.h>
#include <odc/TopologyOpSetProperties.h>
#include <odc/TopologyOpWaitForState.h>

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/system_executor.hpp>

#include <dds/Tools.h>
#include <dds/Topology.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odc::core
{

/**
 * @class BasicTopology
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
    BasicTopology(dds::topology_api::CTopology& topo, dds::tools_api::CSession& session, bool blockUntilConnected = false)
        : BasicTopology<Executor, Allocator>(boost::asio::system_executor(), topo, session, blockUntilConnected)
    {}

    /// @brief (Re)Construct a FairMQ topology from an existing DDS topology
    /// @param ex I/O executor to be associated
    /// @param topo CTopology
    /// @param session CSession
    /// @param blockUntilConnected if true, ctor will wait for all tasks to confirm subscriptions
    /// @throws RuntimeError
    BasicTopology(const Executor& ex,
                  dds::topology_api::CTopology& topo,
                  dds::tools_api::CSession& ddsSession,
                  bool blockUntilConnected = false,
                  Allocator alloc = DefaultAllocator())
        : AsioBase<Executor, Allocator>(ex, std::move(alloc))
        , mDDSSession(ddsSession)
        , mDDSCustomCmd(mDDSService)
        , mDDSTopo(topo)
        , mMtx(std::make_unique<std::mutex>())
        , mStateChangeSubscriptionsCV(std::make_unique<std::condition_variable>())
        , mNumStateChangePublishers(0)
        , mHeartbeatsTimer(boost::asio::system_executor())
        , mHeartbeatInterval(600000)
    {
        // prepare topology state
        const auto tasks = GetTasks("", true);
        mStateData.reserve(tasks.size());

        int index = 0;

        for (const auto& task : tasks) {
            bool expendable = expendableTasks.find(task.GetId()) != expendableTasks.end();

            mStateData.push_back(DeviceStatus(expendable, task.GetId(), task.GetCollectionId()));
            mStateIndex.emplace(task.GetId(), index);
            index++;
        }

        SubscribeToCommands();
        SubscribeToTaskDoneEvents();

        mDDSService.start(to_string(mDDSSession.getSessionID()));
        SubscribeToStateChanges();
        if (blockUntilConnected) {
            WaitForPublisherCount(mStateIndex.size());
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

        mDDSCustomCmd.unsubscribe();
        try {
            std::lock_guard<std::mutex> lk(*mMtx);
            for (auto& op : mChangeStateOps) {
                op.second.Complete(MakeErrorCode(ErrorCode::OperationCanceled));
            }
        } catch (...) {
        }
        mDDSOnTaskDoneRequest->unsubscribeResponseCallback();
    }

    // precondition: mMtx is locked.
    std::vector<DDSTask> GetTasks(const std::string& path = "", bool firstRun = false) const
    {
        std::vector<DDSTask> list;

        dds::topology_api::STopoRuntimeTask::FilterIteratorPair_t itPair;
        if (path.empty()) {
            itPair = mDDSTopo.getRuntimeTaskIterator(nullptr); // passing nullptr will get all tasks
        } else {
            itPair = mDDSTopo.getRuntimeTaskIteratorMatchingPath(path);
        }
        auto tasks = boost::make_iterator_range(itPair.first, itPair.second);

        list.reserve(boost::size(tasks));

        // std::cout << "GetTasks(): Num of tasks: " << boost::size(tasks) << std::endl;
        for (const auto& task : tasks) {
            // std::cout << "GetTasks(): Found task with id: " << task.first << ", "
            //            << "Path: " << task.second.m_taskPath << ", "
            //            << "Collection id: " << task.second.m_taskCollectionId << ", "
            //            << "Name: " << task.second.m_task->getName() << "_" << task.second.m_taskIndex << std::endl;
            if (!firstRun) {
                const DeviceStatus& ds = mStateData.at(mStateIndex.at(task.first));
                if (ds.ignored) {
                    // std::cout << "GetTasks(): Task " << ds.taskId << " has failed and is set to be ignored, skipping" << std::endl;
                    continue;
                }
            }
            list.emplace_back(task.first, task.second.m_taskCollectionId);
        }

        return list;
    }

    void IgnoreFailedTask(uint64_t id)
    {
        std::lock_guard<std::mutex> lk(*mMtx);
        DeviceStatus& device = mStateData.at(mStateIndex.at(id));
        if (device.subscribedToStateChanges) {
            device.subscribedToStateChanges = false;
            --mNumStateChangePublishers;
        }
        device.ignored = true;
    }

    void IgnoreFailedCollections(const std::vector<CollectionDetails*>& collections)
    {
        std::lock_guard<std::mutex> lk(*mMtx);
        for (auto& device : mStateData) {
            for (const auto& collection : collections) {
                if (device.collectionId == collection->mCollectionID) {
                    // std::cout << "Ignoring device " << device.taskId << " from collection " << collection->mCollectionID << std::endl;
                    if (device.subscribedToStateChanges) {
                        device.subscribedToStateChanges = false;
                        --mNumStateChangePublishers;
                    }
                    device.ignored = true;
                }
            }
        }
    }

    void SubscribeToStateChanges()
    {
        // FAIR_LOG(debug) << "Subscribing to state change";
        cc::Cmds cmds(cc::make<cc::SubscribeToStateChange>(mHeartbeatInterval.count()));
        mDDSCustomCmd.send(cmds.Serialize(), "");

        mHeartbeatsTimer.expires_after(mHeartbeatInterval);
        mHeartbeatsTimer.async_wait(std::bind(&BasicTopology::SendSubscriptionHeartbeats, this, std::placeholders::_1));
    }

    void SubscribeToTaskDoneEvents()
    {
        using namespace dds::tools_api;
        SOnTaskDoneRequest::request_t request;
        mDDSOnTaskDoneRequest = SOnTaskDoneRequest::makeRequest(request);
        mDDSOnTaskDoneRequest->setResponseCallback([&](const SOnTaskDoneResponseData& info) {
            std::unique_lock<std::mutex> lk(*mMtx);
            DeviceStatus& task = mStateData.at(mStateIndex.at(info.m_taskID));
            if (task.subscribedToStateChanges) {
                task.subscribedToStateChanges = false;
                --mNumStateChangePublishers;
            }
            task.exitCode = info.m_exitCode;
            task.signal = info.m_signal;
            task.lastState = task.state;
            if (task.exitCode > 0) {
                task.state = DeviceState::Error;
            } else {
                task.state = DeviceState::Exiting;
            }

            for (auto& op : mChangeStateOps) {
                op.second.Update(task.taskId, task.state);
            }
            for (auto& op : mWaitForStateOps) {
                op.second.Update(task.taskId, task.lastState, task.state);
            }
            // std::cout << "task " << task.taskId << " exited" << std::endl;
            // TODO: include set/get property ops
        });
        mDDSSession.sendRequest<SOnTaskDoneRequest>(mDDSOnTaskDoneRequest);
    }

    void WaitForPublisherCount(unsigned int number)
    {
        using namespace std::chrono_literals;
        std::unique_lock<std::mutex> lk(*mMtx);
        auto publisherCountReached = [&]() { return mNumStateChangePublishers == number; };
        auto count = 0;
        constexpr auto checkInterval(50ms);
        constexpr auto maxCount(30s / checkInterval);
        while (!publisherCountReached() && mDDSSession.IsRunning() && count < maxCount) {
            mStateChangeSubscriptionsCV->wait_for(lk, checkInterval, publisherCountReached);
            ++count;
        }
    }

    void SendSubscriptionHeartbeats(const boost::system::error_code& ec)
    {
        if (!ec) {
            // Timer expired.
            mDDSCustomCmd.send(cc::Cmds(cc::make<cc::SubscriptionHeartbeat>(mHeartbeatInterval.count())).Serialize(), "");
            // schedule again
            mHeartbeatsTimer.expires_after(mHeartbeatInterval);
            mHeartbeatsTimer.async_wait(std::bind(&BasicTopology::SendSubscriptionHeartbeats, this, std::placeholders::_1));
        } else if (ec == boost::asio::error::operation_aborted) {
            // OLOG(debug) << "Heartbeats timer canceled";
        } else {
            OLOG(error) << "Timer error: " << ec;
        }
    }

    void UnsubscribeFromStateChanges()
    {
        // stop sending heartbeats
        mHeartbeatsTimer.cancel();
        // unsubscribe from state changes
        mDDSCustomCmd.send(cc::Cmds(cc::make<cc::UnsubscribeFromStateChange>()).Serialize(), "");
        // wait for all tasks to confirm unsubscription
        WaitForPublisherCount(0);
    }

    void SubscribeToCommands()
    {
        mDDSCustomCmd.subscribe([&](const std::string& msg, const std::string& /* condition */, uint64_t ddsSenderChannelId) {
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
                        HandleCmd(static_cast<cc::StateChange&>(*cmd));
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

    void HandleCmd(cc::StateChangeSubscription const& cmd)
    {
        if (cmd.GetResult() == cc::Result::Ok) {
            DDSTask::Id taskId(cmd.GetTaskId());

            try {
                std::unique_lock<std::mutex> lk(*mMtx);
                DeviceStatus& task = mStateData.at(mStateIndex.at(taskId));
                if (!task.subscribedToStateChanges) {
                    task.subscribedToStateChanges = true;
                    ++mNumStateChangePublishers;
                } else {
                    OLOG(warning) << "Task '" << task.taskId << "' sent subscription confirmation more than once";
                }
                lk.unlock();
                mStateChangeSubscriptionsCV->notify_one();
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
                std::unique_lock<std::mutex> lk(*mMtx);
                DeviceStatus& task = mStateData.at(mStateIndex.at(taskId));
                if (task.subscribedToStateChanges) {
                    task.subscribedToStateChanges = false;
                    --mNumStateChangePublishers;
                } else {
                    // OLOG(debug) << "Task '" << task.taskId << "' sent unsubscription confirmation more than once";
                }
                lk.unlock();
                mStateChangeSubscriptionsCV->notify_one();
            } catch (const std::exception& e) {
                OLOG(error) << "Exception in HandleCmd(cc::StateChangeUnsubscription const&): " << e.what();
            }
        } else {
            OLOG(error) << "State change unsubscription failed for device: " << cmd.GetDeviceId() << ", task id: " << cmd.GetTaskId();
        }
    }

    void HandleCmd(cc::StateChange const& cmd)
    {
        DDSTask::Id taskId(cmd.GetTaskId());

        try {
            std::lock_guard<std::mutex> lk(*mMtx);
            DeviceStatus& task = mStateData.at(mStateIndex.at(taskId));
            task.lastState = cmd.GetLastState();
            task.state = cmd.GetCurrentState();
            // std::cout << "Updated state entry: taskId=" << taskId << ", state=" << task.state << std::endl;

            for (auto& op : mChangeStateOps) {
                op.second.Update(taskId, cmd.GetCurrentState());
            }
            for (auto& op : mWaitForStateOps) {
                op.second.Update(taskId, cmd.GetLastState(), cmd.GetCurrentState());
            }
        } catch (const std::exception& e) {
            OLOG(error) << "Exception in HandleCmd(cmd::StateChange const&): " << e.what();
            OLOG(error) << "Possibly no task with id '" << taskId << "'?";
        }
    }

    void HandleCmd(cc::TransitionStatus const& cmd)
    {
        if (cmd.GetResult() != cc::Result::Ok) {
            DDSTask::Id taskId(cmd.GetTaskId());
            std::lock_guard<std::mutex> lk(*mMtx);
            for (auto& op : mChangeStateOps) {
                if (!op.second.IsCompleted() && op.second.ContainsTask(taskId)) {
                    if (mStateData.at(mStateIndex.at(taskId)).state != op.second.GetTargetState()) {
                        OLOG(error) << cmd.GetTransition() << " transition failed for " << cmd.GetDeviceId() << ", device is in " << cmd.GetCurrentState() << " state.";
                        op.second.Complete(MakeErrorCode(ErrorCode::DeviceChangeStateInvalidTransition));
                    } else {
                        OLOG(debug) << cmd.GetTransition() << " transition failed for " << cmd.GetDeviceId() << ", device is already in " << cmd.GetCurrentState() << " state.";
                    }
                }
            }
        }
    }

    void HandleCmd(cc::Properties const& cmd)
    {
        std::unique_lock<std::mutex> lk(*mMtx);
        try {
            auto& op(mGetPropertiesOps.at(cmd.GetRequestId()));
            lk.unlock();
            op.Update(cmd.GetTaskId(), cmd.GetResult(), cmd.GetProps());
        } catch (std::out_of_range& e) {
            OLOG(debug) << "GetProperties operation (request id: " << cmd.GetRequestId() << ") not found (probably completed or timed out), "
                        << "discarding reply of device " << cmd.GetDeviceId() << ", task id: " << cmd.GetTaskId();
        }
    }

    void HandleCmd(cc::PropertiesSet const& cmd)
    {
        std::unique_lock<std::mutex> lk(*mMtx);
        try {
            auto& op(mSetPropertiesOps.at(cmd.GetRequestId()));
            lk.unlock();
            op.Update(cmd.GetTaskId(), cmd.GetResult());
        } catch (std::out_of_range& e) {
            OLOG(debug) << "SetProperties operation (request id: " << cmd.GetRequestId() << ") not found (probably completed or timed out), "
                        << "discarding reply of device " << cmd.GetDeviceId() << ", task id: " << cmd.GetTaskId();
        }
    }

    /// @brief Initiate state transition on all FairMQ devices in this topology
    /// @param transition FairMQ device state machine transition
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncChangeState(const TopoTransition transition, const std::string& path, Duration timeout, CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, ChangeStateCompletionSignature>(
            [&](auto handler) {
                const uint64_t id = uuidHash();

                std::lock_guard<std::mutex> lk(*mMtx);

                for (auto it = begin(mChangeStateOps); it != end(mChangeStateOps);) {
                    if (it->second.IsCompleted()) {
                        it = mChangeStateOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                auto [it, inserted] = mChangeStateOps.try_emplace(id,
                                                                  id,
                                                                  transition,
                                                                  GetTasks(path),
                                                                  mStateData,
                                                                  timeout,
                                                                  *mMtx,
                                                                  AsioBase<Executor, Allocator>::GetExecutor(),
                                                                  AsioBase<Executor, Allocator>::GetAllocator(),
                                                                  std::move(handler)
                );

                cc::Cmds cmds(cc::make<cc::ChangeState>(transition));
                mDDSCustomCmd.send(cmds.Serialize(), path);

                it->second.ResetCount(mStateIndex, mStateData);
                // TODO: make sure following operation properly queues the completion and not doing it directly out of initiation call.
                it->second.TryCompletion();
            },
            token);
    }

    /// @brief Initiate state transition on all FairMQ devices in this topology
    /// @param transition FairMQ device state machine transition
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncChangeState(const TopoTransition transition, CompletionToken&& token)
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
    auto AsyncChangeState(const TopoTransition transition, Duration timeout, CompletionToken&& token)
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
    auto AsyncChangeState(const TopoTransition transition, const std::string& path, CompletionToken&& token)
    {
        return AsyncChangeState(transition, path, Duration(0), std::move(token));
    }

    /// @brief Perform state transition on FairMQ devices in this topology for a specified topology path
    /// @param transition FairMQ device state machine transition
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    std::pair<std::error_code, TopoState> ChangeState(const TopoTransition transition, const std::string& path = "", Duration timeout = Duration(0))
    {
        SharedSemaphore blocker;
        std::error_code ec;
        TopoState state;
        AsyncChangeState(transition, path, timeout, [&, blocker](std::error_code _ec, TopoState _state) mutable {
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
    std::pair<std::error_code, TopoState> ChangeState(const TopoTransition transition, Duration timeout) { return ChangeState(transition, "", timeout); }

    /// @brief Returns the current state of the topology
    /// @return map of id : DeviceStatus
    TopoState GetCurrentState() const
    {
        std::lock_guard<std::mutex> lk(*mMtx);
        return mStateData;
    }

    DeviceState AggregateState() const { return AggregateState(GetCurrentState()); }

    bool StateEqualsTo(DeviceState state) const { return StateEqualsTo(GetCurrentState(), state); }

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
                const uint64_t id = uuidHash();

                std::lock_guard<std::mutex> lk(*mMtx);

                for (auto it = begin(mWaitForStateOps); it != end(mWaitForStateOps);) {
                    if (it->second.IsCompleted()) {
                        it = mWaitForStateOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                auto [it, inserted] = mWaitForStateOps.try_emplace(id,
                                                                   id,
                                                                   targetLastState,
                                                                   targetCurrentState,
                                                                   GetTasks(path),
                                                                   timeout,
                                                                   *mMtx,
                                                                   AsioBase<Executor, Allocator>::GetExecutor(),
                                                                   AsioBase<Executor, Allocator>::GetAllocator(),
                                                                   std::move(handler)
                );

                it->second.ResetCount(mStateIndex, mStateData);
                // TODO: make sure following operation properly queues the completion and not doing it directly out of initiation call.
                it->second.TryCompletion();
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
    std::error_code WaitForState(const DeviceState targetLastState, const DeviceState targetCurrentState, const std::string& path = "", Duration timeout = Duration(0))
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
    std::error_code WaitForState(const DeviceState targetCurrentState, const std::string& path = "", Duration timeout = Duration(0))
    {
        return WaitForState(DeviceState::Undefined, targetCurrentState, path, timeout);
    }

    /// @brief Initiate property query on selected FairMQ devices in this topology
    /// @param query Key(s) to be queried (regex)
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncGetProperties(const std::string& query, const std::string& path, Duration timeout, CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, GetPropertiesCompletionSignature>(
            [&](auto handler) {
                const uint64_t id = uuidHash();

                std::lock_guard<std::mutex> lk(*mMtx);

                for (auto it = begin(mGetPropertiesOps); it != end(mGetPropertiesOps);) {
                    if (it->second.IsCompleted()) {
                        it = mGetPropertiesOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                mGetPropertiesOps.try_emplace(id,
                                              id,
                                              GetTasks(path),
                                              timeout,
                                              *mMtx,
                                              AsioBase<Executor, Allocator>::GetExecutor(),
                                              AsioBase<Executor, Allocator>::GetAllocator(),
                                              std::move(handler)
                );

                cc::Cmds const cmds(cc::make<cc::GetProperties>(id, query));
                mDDSCustomCmd.send(cmds.Serialize(), path);
            },
            token);
    }

    /// @brief Initiate property query on selected FairMQ devices in this topology
    /// @param query Key(s) to be queried (regex)
    /// @param token Asio completion token
    /// @tparam CompletionToken Asio completion token type
    /// @throws std::system_error
    template<typename CompletionToken>
    auto AsyncGetProperties(const std::string& query, CompletionToken&& token)
    {
        return AsyncGetProperties(query, "", Duration(0), std::move(token));
    }

    /// @brief Query properties on selected FairMQ devices in this topology
    /// @param query Key(s) to be queried (regex)
    /// @param path Select a subset of FairMQ devices in this topology, empty selects all
    /// @param timeout Timeout in milliseconds, 0 means no timeout
    /// @throws std::system_error
    std::pair<std::error_code, GetPropertiesResult> GetProperties(const std::string& query, const std::string& path = "", Duration timeout = Duration(0))
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
                const uint64_t id = uuidHash();

                std::lock_guard<std::mutex> lk(*mMtx);

                for (auto it = begin(mGetPropertiesOps); it != end(mGetPropertiesOps);) {
                    if (it->second.IsCompleted()) {
                        it = mGetPropertiesOps.erase(it);
                    } else {
                        ++it;
                    }
                }

                auto [it, inserted] = mSetPropertiesOps.try_emplace(id,
                                                                    id,
                                                                    GetTasks(path),
                                                                    timeout,
                                                                    *mMtx,
                                                                    AsioBase<Executor, Allocator>::GetExecutor(),
                                                                    AsioBase<Executor, Allocator>::GetAllocator(),
                                                                    std::move(handler)
                );

                cc::Cmds const cmds(cc::make<cc::SetProperties>(id, props));
                mDDSCustomCmd.send(cmds.Serialize(), path);

                it->second.ResetCount(mStateIndex, mStateData);
                // TODO: make sure following operation properly queues the completion and not doing it directly out of initiation call.
                it->second.TryCompletion();
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
    std::pair<std::error_code, FailedDevices> SetProperties(DeviceProperties const& properties, const std::string& path = "", Duration timeout = Duration(0))
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

    std::chrono::milliseconds GetHeartbeatInterval() const { return mHeartbeatInterval; }
    void SetHeartbeatInterval(std::chrono::milliseconds duration) { mHeartbeatInterval = duration; }

  private:
    dds::tools_api::CSession& mDDSSession;
    dds::intercom_api::CIntercomService mDDSService;
    dds::intercom_api::CCustomCmd mDDSCustomCmd;
    dds::topology_api::CTopology& mDDSTopo;
    dds::tools_api::SOnTaskDoneRequest::ptr_t mDDSOnTaskDoneRequest;
    TopoState mStateData;
    TopoStateIndex mStateIndex;

    mutable std::unique_ptr<std::mutex> mMtx;

    std::unique_ptr<std::condition_variable> mStateChangeSubscriptionsCV;
    unsigned int mNumStateChangePublishers;
    boost::asio::steady_timer mHeartbeatsTimer;
    std::chrono::milliseconds mHeartbeatInterval;

    std::unordered_map<uint64_t, ChangeStateOp<Executor, Allocator>> mChangeStateOps;
    std::unordered_map<uint64_t, WaitForStateOp<Executor, Allocator>> mWaitForStateOps;
    std::unordered_map<uint64_t, SetPropertiesOp<Executor, Allocator>> mSetPropertiesOps;
    std::unordered_map<uint64_t, GetPropertiesOp<Executor, Allocator>> mGetPropertiesOps;

    /// precodition: mMtx is locked.
    TopoState GetCurrentStateUnsafe() const { return mStateData; }
};

using Topology = BasicTopology<DefaultExecutor, DefaultAllocator>;

} // namespace odc::core

#endif /* ODC_TOPOLOGY */
