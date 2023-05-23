/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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
    GetPropertiesOp(uint64_t id,
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
                    mOp.Timeout(mResult);
                }
            });
        }
        if (mTasks.empty()) {
            OLOG(warning) << "GetProperties initiated on an empty set of tasks, check the path argument.";
        }

        mResult.failed.reserve(mTasks.size());
        for (const auto& taskId : mTasks) {
            mResult.failed.emplace(taskId);
        }

        // OLOG(debug) << "GetProperties " << mId << " with expected count of " << mTasks.size() << " started.";
    }
    GetPropertiesOp() = delete;
    GetPropertiesOp(const GetPropertiesOp&) = delete;
    GetPropertiesOp& operator=(const GetPropertiesOp&) = delete;
    GetPropertiesOp(GetPropertiesOp&&) = default;
    GetPropertiesOp& operator=(GetPropertiesOp&&) = default;
    ~GetPropertiesOp() = default;

    /// precondition: mMtx is locked.
    void Update(const DDSTask::Id taskId, cc::Result result, DeviceProperties props)
    {
        if (result == cc::Result::Ok) {
            mResult.failed.erase(taskId);
            mResult.devices.insert({ taskId, { std::move(props) } });
        }
        ++mCount;
        TryCompletion();
    }

    bool IsCompleted() { return mOp.IsCompleted(); }

  private:
    const uint64_t mId;
    AsioAsyncOp<Executor, Allocator, GetPropertiesCompletionSignature> mOp;
    boost::asio::steady_timer mTimer;
    unsigned int mCount;
    std::unordered_set<DDSTask::Id> mTasks;
    GetPropertiesResult mResult;
    std::mutex& mMtx;

    /// precondition: mMtx is locked.
    void TryCompletion()
    {
        if (!mOp.IsCompleted() && mCount == mTasks.size()) {
            mTimer.cancel();
            if (!mResult.failed.empty()) {
                mOp.Complete(MakeErrorCode(ErrorCode::DeviceGetPropertiesFailed), std::move(mResult));
            } else {
                mOp.Complete(std::move(mResult));
            }
        }
    }
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPGETPROPERTIES */
