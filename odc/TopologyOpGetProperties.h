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
#include <vector>

namespace odc::core
{

using GetPropertiesCompletionSignature = void(std::error_code, GetPropertiesResult);

template<typename Executor, typename Allocator>
struct GetPropertiesOp
{
    template<typename Handler>
    GetPropertiesOp(uint64_t id,
                    std::vector<DDSTask> tasks,
                    Duration timeout,
                    std::mutex& mutex,
                    Executor const& ex,
                    Allocator const& alloc,
                    Handler&& handler)
        : fId(id)
        , fOp(ex, alloc, std::move(handler))
        , fTimer(ex)
        , fCount(0)
        , fTasks(std::move(tasks))
        , fMtx(mutex)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            fTimer.expires_after(timeout);
            fTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(fMtx);
                    fOp.Timeout(fResult);
                }
            });
        }
        if (fTasks.empty()) {
            OLOG(warning) << "GetProperties initiated on an empty set of tasks, check the path argument.";
        }

        fResult.failed.reserve(fTasks.size());
        for (const auto& task : fTasks) {
            fResult.failed.emplace(task.GetId());
        }

        // OLOG(debug) << "GetProperties " << fId << " with expected count of " << fTasks.size() << " started.";
    }
    GetPropertiesOp() = delete;
    GetPropertiesOp(const GetPropertiesOp&) = delete;
    GetPropertiesOp& operator=(const GetPropertiesOp&) = delete;
    GetPropertiesOp(GetPropertiesOp&&) = default;
    GetPropertiesOp& operator=(GetPropertiesOp&&) = default;
    ~GetPropertiesOp() = default;

    void Update(const DDSTask::Id taskId, cc::Result result, DeviceProperties props)
    {
        std::lock_guard<std::mutex> lk(fMtx);
        if (result == cc::Result::Ok) {
            fResult.failed.erase(taskId);
            fResult.devices.insert({ taskId, { std::move(props) } });
        }
        ++fCount;
        TryCompletion();
    }

    bool IsCompleted() { return fOp.IsCompleted(); }

  private:
    const uint64_t fId;
    AsioAsyncOp<Executor, Allocator, GetPropertiesCompletionSignature> fOp;
    boost::asio::steady_timer fTimer;
    unsigned int fCount;
    std::vector<DDSTask> fTasks;
    GetPropertiesResult fResult;
    std::mutex& fMtx;

    /// precondition: fMtx is locked.
    void TryCompletion()
    {
        if (!fOp.IsCompleted() && fCount == fTasks.size()) {
            fTimer.cancel();
            if (!fResult.failed.empty()) {
                fOp.Complete(MakeErrorCode(ErrorCode::DeviceGetPropertiesFailed), std::move(fResult));
            } else {
                fOp.Complete(std::move(fResult));
            }
        }
    }
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPGETPROPERTIES */
