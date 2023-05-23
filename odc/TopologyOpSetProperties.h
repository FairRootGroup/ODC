/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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
    SetPropertiesOp(uint64_t id,
                    std::unordered_set<DDSTask::Id> tasks,
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
        , mMtx(mutex)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            mTimer.expires_after(timeout);
            mTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(mMtx);
                    mOp.Timeout(mOutstandingDevices);
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "SetProperties initiated on an empty set of tasks, check the path argument.";
        }
        // OLOG(debug) << "SetProperties " << mId << " with expected count of " << mTasks.size() << " started.";
    }
    SetPropertiesOp() = delete;
    SetPropertiesOp(const SetPropertiesOp&) = delete;
    SetPropertiesOp& operator=(const SetPropertiesOp&) = delete;
    SetPropertiesOp(SetPropertiesOp&&) = default;
    SetPropertiesOp& operator=(SetPropertiesOp&&) = default;
    ~SetPropertiesOp() = default;

    /// precondition: mMtx is locked.
    void ResetCount(const TopoStateIndex& stateIndex, const TopoState& stateData)
    {
        mOutstandingDevices.reserve(mTasks.size());
        for (const auto& taskId : mTasks) {
            const DeviceStatus& ds = stateData.at(stateIndex.at(taskId));
            // Do not wait for an errored/exited device that is not yet ignored
            if (ds.state == DeviceState::Error || ds.state == DeviceState::Exiting) {
                ++mCount;
            }
            // but always list at as failed/outstanding
            mOutstandingDevices.emplace(taskId);
        }
    }

    /// precondition: mMtx is locked.
    void Update(const DDSTask::Id taskId, cc::Result result, bool expendable)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            if (result == cc::Result::Ok) {
                mOutstandingDevices.erase(taskId);
                ++mCount;
            } else {
                if (mOutstandingDevices.count(taskId) > 0) {
                    // if expendable - ignore it, by not returning an error
                    // TODO: should the failed device still be returned, but with a success condition?
                    if (expendable) {
                        mOutstandingDevices.erase(taskId);
                    }
                    ++mCount;
                } else {
                    // OLOG(debug) << "Task " << taskId << " is already updated";
                }
            }
            TryCompletion();
        }
    }

    /// precondition: mMtx is locked.
    void TryCompletion()
    {
        if (!mOp.IsCompleted() && mCount == mTasks.size()) {
            mTimer.cancel();
            if (!mOutstandingDevices.empty()) {
                mOp.Complete(MakeErrorCode(ErrorCode::DeviceSetPropertiesFailed), mOutstandingDevices);
            } else {
                mOp.Complete(mOutstandingDevices);
            }
        }
    }

    bool IsCompleted() { return mOp.IsCompleted(); }

  private:
    const uint64_t mId;
    AsioAsyncOp<Executor, Allocator, SetPropertiesCompletionSignature> mOp;
    boost::asio::steady_timer mTimer;
    unsigned int mCount;
    std::unordered_set<DDSTask::Id> mTasks;
    FailedDevices mOutstandingDevices;
    std::mutex& mMtx;

    /// precondition: mMtx is locked.
    bool ContainsTask(DDSTask::Id id) { return mTasks.count(id) > 0; }
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPSETPROPERTIES */
