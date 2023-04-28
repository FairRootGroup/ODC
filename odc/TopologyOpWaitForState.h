/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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
#include <vector>

namespace odc::core
{

using WaitForStateCompletionSignature = void(std::error_code);

template<typename Executor, typename Allocator>
struct WaitForStateOp
{
    template<typename Handler>
    WaitForStateOp(uint64_t id,
                   DeviceState targetLastState,
                   DeviceState targetCurrentState,
                   std::vector<DDSTask> tasks,
                   Duration timeout,
                   std::mutex& mutex,
                   Executor const& ex,
                   Allocator const& alloc,
                   Handler&& handler)
        : mId(id)
        , mOp(ex, alloc, std::move(handler))
        , mTimer(ex)
        , mCount(0)
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
                    mOp.Timeout();
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "WaitForState initiated on an empty set of tasks, check the path argument.";
        }
    }
    WaitForStateOp() = delete;
    WaitForStateOp(const WaitForStateOp&) = delete;
    WaitForStateOp& operator=(const WaitForStateOp&) = delete;
    WaitForStateOp(WaitForStateOp&&) = default;
    WaitForStateOp& operator=(WaitForStateOp&&) = default;
    ~WaitForStateOp() = default;

    /// precondition: mMtx is locked.
    void ResetCount(const TopoStateIndex& stateIndex, const TopoState& stateData)
    {
        mCount = std::count_if(stateIndex.cbegin(), stateIndex.cend(), [=](const auto& s) {
            const auto& task = stateData.at(s.second);
            if (ContainsTask(task.taskId)) {
                if (task.state == mTargetCurrentState && (task.lastState == mTargetLastState || mTargetLastState == DeviceState::Undefined)) {
                    return true;
                } else {
                    // Do not wait for an errored/exited device that is not yet ignored
                    if (task.state == DeviceState::Error || task.state == DeviceState::Exiting) {
                        mErrored = true;
                        return true;
                    } else {
                        return false;
                    }
                }
            } else {
                return false;
            }
        });
    }

    /// precondition: mMtx is locked.
    void Update(const DDSTask::Id taskId, const DeviceState lastState, const DeviceState currentState, bool expendable)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            if (currentState == mTargetCurrentState && (lastState == mTargetLastState || mTargetLastState == DeviceState::Undefined)) {
                ++mCount;
            } else if (currentState == DeviceState::Error || currentState == DeviceState::Exiting) {
                if (mFailed.count(taskId) == 0) {
                    mErrored = expendable ? false : true;
                    ++mCount;
                    mFailed.emplace(taskId);
                } else {
                    // OLOG(debug) << "Task " << taskId << " is already in the set of failed devices. Called twice from StateChange & onTaskDone?";
                }
            }
            TryCompletion();
        }
    }

    /// precondition: mMtx is locked.
    void TryCompletion()
    {
        if (!mOp.IsCompleted() && mCount == mTasks.size()) {
            if (mErrored) {
                Complete(MakeErrorCode(ErrorCode::DeviceChangeStateFailed));
            } else {
                Complete(std::error_code());
            }
        }
    }

    /// precondition: mMtx is locked.
    void Complete(std::error_code ec)
    {
        mTimer.cancel();
        mOp.Complete(ec);
    }

    /// precondition: mMtx is locked.
    bool ContainsTask(DDSTask::Id id)
    {
        auto it = std::find_if(mTasks.begin(), mTasks.end(), [id](const DDSTask& t) { return t.GetId() == id; });
        return it != mTasks.end();
    }

    bool IsCompleted() { return mOp.IsCompleted(); }

  private:
    const uint64_t mId;
    AsioAsyncOp<Executor, Allocator, WaitForStateCompletionSignature> mOp;
    boost::asio::steady_timer mTimer;
    unsigned int mCount;
    std::vector<DDSTask> mTasks;
    FailedDevices mFailed;
    DeviceState mTargetLastState;
    DeviceState mTargetCurrentState;
    std::mutex& mMtx;
    bool mErrored = false;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPWAITFORSTATE */
