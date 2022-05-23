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
    {}

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

    // precondition: fMtx is locked.
    std::vector<DDSTask> GetTasks(const std::string& path = "", bool firstRun = false) const
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

        // std::cout << "GetTasks(): Num of tasks: " << boost::size(tasks) << std::endl;
        for (const auto& task : tasks) {
            // std::cout << "GetTasks(): Found task with id: " << task.first << ", "
            //            << "Path: " << task.second.m_taskPath << ", "
            //            << "Collection id: " << task.second.m_taskCollectionId << ", "
            //            << "Name: " << task.second.m_task->getName() << "_" << task.second.m_taskIndex << std::endl;
            if (!firstRun) {
                const DeviceStatus& ds = fStateData.at(fStateIndex.at(task.first));
                if (ds.ignored) {
                    // OLOG(debug) << "GetTasks(): Task " << ds.taskId << " has failed and is set to be ignored, skipping";
                    continue;
                }
            }
            list.emplace_back(task.first, task.second.m_taskCollectionId);
        }

        return list;
    }

    void IgnoreFailedTask(uint64_t id)
    {
        std::lock_guard<std::mutex> lk(*fMtx);
        DeviceStatus& device = fStateData.at(fStateIndex.at(id));
        if (device.subscribedToStateChanges) {
            device.subscribedToStateChanges = false;
            --fNumStateChangePublishers;
        }
        device.ignored = true;
    }

    void IgnoreFailedCollections(const std::vector<CollectionDetails*>& collections)
    {
        std::lock_guard<std::mutex> lk(*fMtx);
        for (auto& device : fStateData) {
            for (const auto& collection : collections) {
                if (device.collectionId == collection->mCollectionID) {
                    // std::cout << "Ignoring device " << device.taskId << " from collection " << collection->mCollectionID << std::endl;
                    if (device.subscribedToStateChanges) {
                        device.subscribedToStateChanges = false;
                        --fNumStateChangePublishers;
                    }
                    device.ignored = true;
                }
            }
        }
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
            if (task.exitCode > 0) {
                task.state = DeviceState::Error;
            } else {
                task.state = DeviceState::Exiting;
            }

            for (auto& op : fChangeStateOps) {
                op.second.Update(task.taskId, task.state);
            }
            for (auto& op : fWaitForStateOps) {
                op.second.Update(task.taskId, task.lastState, task.state);
            }
            // TODO: include set/get property ops
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
                    // OLOG(debug) << "Task '" << task.taskId << "' sent unsubscription confirmation more than once";
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

    void HandleCmd(cc::StateChange const& cmd)
    {
        DDSTask::Id taskId(cmd.GetTaskId());

        try {
            std::lock_guard<std::mutex> lk(*fMtx);
            DeviceStatus& task = fStateData.at(fStateIndex.at(taskId));
            task.lastState = cmd.GetLastState();
            task.state = cmd.GetCurrentState();
            // OLOG(debug) << "Updated state entry: taskId=" << taskId << ", state=" << task.state;

            for (auto& op : fChangeStateOps) {
                op.second.Update(taskId, cmd.GetCurrentState());
            }
            for (auto& op : fWaitForStateOps) {
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

    void HandleCmd(cc::Properties const& cmd)
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

    void HandleCmd(cc::PropertiesSet const& cmd)
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
    ///     odc::core::TopoTransition::InitDevice,
    ///     std::chrono::milliseconds(500),
    ///     [](std::error_code ec, odc::core::TopoState state) {
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
    /// auto fut = topo.AsyncChangeState(odc::core::TopoTransition::InitDevice,
    ///                                  std::chrono::milliseconds(500),
    ///                                  boost::asio::use_future);
    /// try {
    ///     odc::core::TopoState state = fut.get();
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
    ///     odc::core::TopoState state = co_await
    ///         topo.AsyncChangeState(odc::core::TopoTransition::InitDevice,
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
    auto AsyncChangeState(const TopoTransition transition, const std::string& path, Duration timeout, CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, ChangeStateCompletionSignature>(
            [&](auto handler) {
                const uint64_t id(uuidHash());

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
        std::lock_guard<std::mutex> lk(*fMtx);
        return fStateData;
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
                const uint64_t id(uuidHash());

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
                const uint64_t id(uuidHash());

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
                const uint64_t id(uuidHash());

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

    std::chrono::milliseconds GetHeartbeatInterval() const { return fHeartbeatInterval; }
    void SetHeartbeatInterval(std::chrono::milliseconds duration) { fHeartbeatInterval = duration; }

  private:
    std::shared_ptr<dds::tools_api::CSession> fDDSSession;
    dds::intercom_api::CIntercomService fDDSService;
    dds::intercom_api::CCustomCmd fDDSCustomCmd;
    dds::topology_api::CTopology fDDSTopo;
    dds::tools_api::SOnTaskDoneRequest::ptr_t fDDSOnTaskDoneRequest;
    TopoState fStateData;
    TopoStateIndex fStateIndex;

    mutable std::unique_ptr<std::mutex> fMtx;

    std::unique_ptr<std::condition_variable> fStateChangeSubscriptionsCV;
    unsigned int fNumStateChangePublishers;
    boost::asio::steady_timer fHeartbeatsTimer;
    std::chrono::milliseconds fHeartbeatInterval;

    std::unordered_map<uint64_t, ChangeStateOp<Executor, Allocator>> fChangeStateOps;
    std::unordered_map<uint64_t, WaitForStateOp<Executor, Allocator>> fWaitForStateOps;
    std::unordered_map<uint64_t, SetPropertiesOp<Executor, Allocator>> fSetPropertiesOps;
    std::unordered_map<uint64_t, GetPropertiesOp<Executor, Allocator>> fGetPropertiesOps;

    void makeTopologyState()
    {
        const auto tasks = GetTasks("", true);
        fStateData.reserve(tasks.size());

        int index = 0;

        for (const auto& task : tasks) {
            fStateData.push_back(DeviceStatus{ false, false, DeviceState::Undefined, DeviceState::Undefined, task.GetId(), task.GetCollectionId(), -1, -1 });
            fStateIndex.emplace(task.GetId(), index);
            index++;
        }
    }

    /// precodition: fMtx is locked.
    TopoState GetCurrentStateUnsafe() const { return fStateData; }
};

using Topology = BasicTopology<DefaultExecutor, DefaultAllocator>;

} // namespace odc::core

#endif /* ODC_TOPOLOGY */
