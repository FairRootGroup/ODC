/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_TOPOLOGYOPGETPROPERTIES
#define ODC_TOPOLOGYOPGETPROPERTIES

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

using GetPropertiesCompletionSignature = void(std::error_code, GetPropertiesResult);

template<typename Executor, typename Allocator>
struct GetPropertiesOp
{
    template<typename Handler>
    GetPropertiesOp(std::unordered_set<DDSTaskId> tasks,
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
                        for (const auto& taskId : mTasks) {
                            mResult.failed.emplace(taskId);
                        }
                        mOp.Timeout(mResult);
                    }
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "GetProperties initiated on an empty set of tasks, check the path argument.";
        }
    }
    GetPropertiesOp() = delete;
    GetPropertiesOp(const GetPropertiesOp&) = delete;
    GetPropertiesOp& operator=(const GetPropertiesOp&) = delete;
    GetPropertiesOp(GetPropertiesOp&&) = default;
    GetPropertiesOp& operator=(GetPropertiesOp&&) = default;
    ~GetPropertiesOp() = default;

    /// precondition: mMtx is locked.
    void Update(const DDSTaskId taskId, cc::Result result, DeviceProperties props)
    {
        if (!mOp.IsCompleted() && ContainsTask(taskId)) {
            if (result == cc::Result::Ok) {
                mResult.devices.insert({ taskId, { std::move(props) } });
            } else {
                mResult.failed.emplace(taskId);
            }
            mTasks.erase(taskId);
            TryCompletion();
        }
    }

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
            mTimer.cancel();
            if (!mResult.failed.empty()) {
                Complete(MakeErrorCode(ErrorCode::DeviceGetPropertiesFailed));
            } else {
                Complete(std::error_code());
            }
        }
    }

    /// precondition: mMtx is locked.
    void Complete(std::error_code ec)
    {
        mTimer.cancel();
        mOp.Complete(ec, std::move(mResult));
    }

    /// precondition: mMtx is locked.
    bool ContainsTask(DDSTaskId id) { return mTasks.count(id) > 0; }

    bool IsCompleted() { return mOp.IsCompleted(); }

  private:
    AsioAsyncOp<Executor, Allocator, GetPropertiesCompletionSignature> mOp;
    TimeoutHandler mTimeoutHandler;
    boost::asio::steady_timer mTimer;
    std::unordered_set<DDSTaskId> mTasks;
    GetPropertiesResult mResult;
    std::mutex& mMtx;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPGETPROPERTIES */
