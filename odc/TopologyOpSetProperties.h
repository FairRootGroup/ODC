/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_TOPOLOGYOPSETPROPERTIES
#define ODC_TOPOLOGYOPSETPROPERTIES

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

using SetPropertiesCompletionSignature = void(std::error_code, FailedDevices);

template<typename Executor, typename Allocator>
struct SetPropertiesOp
{
    template<typename Handler>
    SetPropertiesOp(std::unordered_set<DDSTaskId> tasks,
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
        , mMtx(mutex)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            mTimer.expires_after(timeout);
            mTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(mMtx);
                    mTimeoutHandler(mTasks);
                    if (!mOp.IsCompleted()) {
                        for (const auto& t : mTasks) {
                            mFailed.emplace(t);
                        }
                        mOp.Timeout(mFailed);
                    }
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "SetProperties initiated on an empty set of tasks, check the path argument.";
        }
        for (auto it = mTasks.begin(); it != mTasks.end();) {
            const DeviceStatus& ds = stateData.at(stateIndex.at(*it));
            if (ds.state == DeviceState::Error || ds.state == DeviceState::Exiting) {
                // Do not wait for an errored/exited device that is not yet ignored (op is started without ignored devices)
                mErrored = true;
                mFailed.emplace(ds.taskId);
                it = mTasks.erase(it);
            } else {
                ++it;
            }
        }
    }
    SetPropertiesOp() = delete;
    SetPropertiesOp(const SetPropertiesOp&) = delete;
    SetPropertiesOp& operator=(const SetPropertiesOp&) = delete;
    SetPropertiesOp(SetPropertiesOp&&) = default;
    SetPropertiesOp& operator=(SetPropertiesOp&&) = default;
    ~SetPropertiesOp() = default;

    /// precondition: mMtx is locked.
    void Update(const DDSTaskId taskId, cc::Result result, bool expendable)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            if (result != cc::Result::Ok) {
                // if expendable - ignore it, by not returning an error
                mErrored = expendable ? false : true;
                mFailed.emplace(taskId);
            }
            mTasks.erase(taskId);
            TryCompletion();
        }
    }

    /// precondition: mMtx is locked.
    void Ignore(const DDSTaskId taskId)
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
                Complete(MakeErrorCode(ErrorCode::DeviceSetPropertiesFailed));
            } else {
                Complete(std::error_code());
            }
        }
    }

    /// precondition: mMtx is locked.
    void Complete(std::error_code ec)
    {
        mTimer.cancel();
        mOp.Complete(ec, mFailed);
    }

    /// precondition: mMtx is locked.
    bool ContainsTask(DDSTaskId id) { return mTasks.count(id) > 0; }

    bool IsCompleted() { return mOp.IsCompleted(); }

  private:
    AsioAsyncOp<Executor, Allocator, SetPropertiesCompletionSignature> mOp;
    TimeoutHandler mTimeoutHandler;
    boost::asio::steady_timer mTimer;
    std::unordered_set<DDSTaskId> mTasks;
    FailedDevices mFailed;
    std::mutex& mMtx;
    bool mErrored = false;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPSETPROPERTIES */
