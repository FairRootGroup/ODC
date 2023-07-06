/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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
    ChangeStateOp(TopoTransition transition,
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
        , mStateData(stateData)
        , mTimer(ex)
        , mTasks(std::move(tasks))
        , mTargetState(gExpectedState.at(transition))
        , mMtx(mutex)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            mTimer.expires_after(timeout);
            mTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(mMtx);
                    mTimeoutHandler(mTasks);
                    if (!mOp.IsCompleted()) {
                        mOp.Timeout(mStateData);
                    }
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "ChangeState initiated on an empty set of tasks, check the path argument.";
        }

        for (auto it = mTasks.begin(); it != mTasks.end();) {
            const DeviceStatus& ds = stateData.at(stateIndex.at(*it));
            if (ds.state == mTargetState) {
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
    ChangeStateOp() = delete;
    ChangeStateOp(const ChangeStateOp&) = delete;
    ChangeStateOp& operator=(const ChangeStateOp&) = delete;
    ChangeStateOp(ChangeStateOp&&) = default;
    ChangeStateOp& operator=(ChangeStateOp&&) = default;
    ~ChangeStateOp() = default;

    /// precondition: mMtx is locked.
    void Update(const DDSTask::Id taskId, const DeviceState currentState, bool expendable)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            if (currentState == mTargetState) {
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
    AsioAsyncOp<Executor, Allocator, ChangeStateCompletionSignature> mOp;
    TimeoutHandler mTimeoutHandler;
    TopoState& mStateData;
    boost::asio::steady_timer mTimer;
    std::unordered_set<DDSTask::Id> mTasks;
    DeviceState mTargetState;
    std::mutex& mMtx;
    bool mErrored = false;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPCHANGESTATE */
