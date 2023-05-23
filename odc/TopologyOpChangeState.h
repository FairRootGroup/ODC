/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_TOPOLOGYOPCHANGESTATE
#define ODC_TOPOLOGYOPCHANGESTATE

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

using ChangeStateCompletionSignature = void(std::error_code, TopoState);

template<typename Executor, typename Allocator>
struct ChangeStateOp
{
    template<typename Handler>
    ChangeStateOp(uint64_t id,
                  TopoTransition transition,
                  std::unordered_set<DDSTask::Id> tasks,
                  TopoState& stateData,
                  Duration timeout,
                  std::mutex& mutex,
                  Executor const& ex,
                  Allocator const& alloc,
                  Handler&& handler)
        : mId(id)
        , mOp(ex, alloc, std::move(handler))
        , mStateData(stateData)
        , mTimer(ex)
        , mCount(0)
        , mTasks(std::move(tasks))
        , mTargetState(gExpectedState.at(transition))
        , mMtx(mutex)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            mTimer.expires_after(timeout);
            mTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(mMtx);
                    mOp.Timeout(mStateData);
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "ChangeState initiated on an empty set of tasks, check the path argument.";
        }
        // OLOG(debug) << "SetProperties " << mId << " with expected count of " << mTasks.size() << " started.";
    }
    ChangeStateOp() = delete;
    ChangeStateOp(const ChangeStateOp&) = delete;
    ChangeStateOp& operator=(const ChangeStateOp&) = delete;
    ChangeStateOp(ChangeStateOp&&) = default;
    ChangeStateOp& operator=(ChangeStateOp&&) = default;
    ~ChangeStateOp() = default;

    /// precondition: mMtx is locked.
    void ResetCount(const TopoStateIndex& stateIndex, const TopoState& stateData)
    {
        mCount = std::count_if(stateIndex.cbegin(), stateIndex.cend(), [=](const auto& s) {
            const auto& task = stateData.at(s.second);
            if (ContainsTask(task.taskId)) {
                if (task.state == mTargetState) {
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
    void Update(const DDSTask::Id taskId, const DeviceState currentState, bool expendable)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            if (currentState == mTargetState) {
                ++mCount;
            } else if (currentState == DeviceState::Error || currentState == DeviceState::Exiting) {
                if (mFailed.count(taskId) == 0) {
                    mErrored = expendable ? false : true;
                    ++mCount;
                    mFailed.emplace(taskId);
                } else {
                    // OLOG(debug) << "Task " << taskId << " is already in the set of failed devices";
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
        mOp.Complete(ec, mStateData);
    }

    /// precondition: mMtx is locked.
    bool ContainsTask(DDSTask::Id id) { return mTasks.count(id) > 0; }

    bool IsCompleted() { return mOp.IsCompleted(); }

    DeviceState GetTargetState() const { return mTargetState; }

  private:
    const uint64_t mId;
    AsioAsyncOp<Executor, Allocator, ChangeStateCompletionSignature> mOp;
    TopoState& mStateData;
    boost::asio::steady_timer mTimer;
    unsigned int mCount;
    std::unordered_set<DDSTask::Id> mTasks;
    FailedDevices mFailed;
    DeviceState mTargetState;
    std::mutex& mMtx;
    bool mErrored = false;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPCHANGESTATE */
