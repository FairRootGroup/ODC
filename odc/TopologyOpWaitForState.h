/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_TOPOLOGYOPWAITFORSTATE
#define ODC_TOPOLOGYOPWAITFORSTATE

#include <odc/AsioAsyncOp.h>
#include <odc/Error.h>
#include <odc/TopologyDefs.h>

#include <boost/asio/steady_timer.hpp>

#include <dds/Tools.h>
#include <dds/Topology.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <utility>
#include <unordered_set>

namespace odc::core
{

using WaitForStateCompletionSignature = void(std::error_code, FailedDevices);

template<typename Executor, typename Allocator>
struct WaitForStateOp
{
    template<typename Handler>
    WaitForStateOp(DeviceState targetLastState,
                   DeviceState targetCurrentState,
                   std::unordered_set<DDSTask::Id> tasks,
                   const TopoStateIndex& stateIndex,
                   TopoState& stateData,
                   Duration timeout,
                   std::mutex& mutex,
                   TimeoutHandler timeoutHandler,
                   Executor const& ex,
                   Allocator const& alloc,
                   Handler&& handler)
        : mOp(ex, alloc, std::move(handler))
        , mTimeoutHandler(std::move(timeoutHandler))
        , mTimer(ex)
        , mTasks(std::move(tasks))
        , mTargetLastState(targetLastState)
        , mTargetCurrentState(targetCurrentState)
        , mMtx(mutex)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            mTimer.expires_after(timeout);
            mTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(mMtx);
                    mTimeoutHandler(mTasks);
                    if (!mOp.IsCompleted()) {
                        mOp.Timeout(mTasks);
                    }
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "WaitForState initiated on an empty set of tasks, check the path argument.";
        }
        for (auto it = mTasks.begin(); it != mTasks.end();) {
            const DeviceStatus& ds = stateData.at(stateIndex.at(*it));
            if (ds.state == mTargetCurrentState && (ds.lastState == mTargetLastState || mTargetLastState == DeviceState::Undefined)) {
                it = mTasks.erase(it);
            } else if (ds.state == DeviceState::Error || ds.state == DeviceState::Exiting) {
                // Do not wait for an errored/exited device that is not yet ignored (op is started without ignored devices)
                mErrored = true;
                it = mTasks.erase(it);
            } else {
                ++it;
            }
        }
    }
    WaitForStateOp() = delete;
    WaitForStateOp(const WaitForStateOp&) = delete;
    WaitForStateOp& operator=(const WaitForStateOp&) = delete;
    WaitForStateOp(WaitForStateOp&&) = default;
    WaitForStateOp& operator=(WaitForStateOp&&) = default;
    ~WaitForStateOp() = default;

    /// precondition: mMtx is locked.
    void Update(const DDSTask::Id taskId, const DeviceState lastState, const DeviceState currentState, bool expendable)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            if (currentState == mTargetCurrentState && (lastState == mTargetLastState || mTargetLastState == DeviceState::Undefined)) {
                mTasks.erase(taskId);
            } else if (currentState == DeviceState::Error || currentState == DeviceState::Exiting) {
                // if expendable - ignore it, by not returning an error
                mErrored = expendable ? false : true;
                mTasks.erase(taskId);
            }
            TryCompletion();
        }
    }

    /// precondition: mMtx is locked.
    void Ignore(const DDSTask::Id taskId)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            mTasks.erase(taskId);
            TryCompletion();
        }
    }

    /// precondition: mMtx is locked.
    void TryCompletion()
    {
        if (!mOp.IsCompleted() && mTasks.empty()) {
            if (mErrored) {
                Complete(MakeErrorCode(ErrorCode::DeviceWaitForStateFailed));
            } else {
                Complete(std::error_code());
            }
        }
    }

    /// precondition: mMtx is locked.
    void Complete(std::error_code ec)
    {
        mTimer.cancel();
        mOp.Complete(ec, mTasks);
    }

    /// precondition: mMtx is locked.
    bool ContainsTask(DDSTask::Id id) { return mTasks.count(id) > 0; }

    bool IsCompleted() { return mOp.IsCompleted(); }

  private:
    AsioAsyncOp<Executor, Allocator, WaitForStateCompletionSignature> mOp;
    TimeoutHandler mTimeoutHandler;
    boost::asio::steady_timer mTimer;
    std::unordered_set<DDSTask::Id> mTasks;
    DeviceState mTargetLastState;
    DeviceState mTargetCurrentState;
    std::mutex& mMtx;
    bool mErrored = false;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPWAITFORSTATE */
