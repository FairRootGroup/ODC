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
#include <vector>

namespace odc::core
{

using SetPropertiesCompletionSignature = void(std::error_code, FailedDevices);

template<typename Executor, typename Allocator>
struct SetPropertiesOp
{
    template<typename Handler>
    SetPropertiesOp(uint64_t id,
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
                    fOp.Timeout(fOutstandingDevices);
                }
            });
        }
        if (fTasks.empty()) {
            OLOG(warning) << "SetProperties initiated on an empty set of tasks, check the path argument.";
        }
        // OLOG(debug) << "SetProperties " << fId << " with expected count of " << fTasks.size() << " started.";
    }
    SetPropertiesOp() = delete;
    SetPropertiesOp(const SetPropertiesOp&) = delete;
    SetPropertiesOp& operator=(const SetPropertiesOp&) = delete;
    SetPropertiesOp(SetPropertiesOp&&) = default;
    SetPropertiesOp& operator=(SetPropertiesOp&&) = default;
    ~SetPropertiesOp() = default;

    /// precondition: fMtx is locked.
    void ResetCount(const TopoStateIndex& stateIndex, const TopoState& stateData)
    {
        fOutstandingDevices.reserve(fTasks.size());
        for (const auto& task : fTasks) {
            const DeviceStatus& ds = stateData.at(stateIndex.at(task.GetId()));
            // Do not wait for an errored/exited device that is not yet ignored
            if (ds.state == DeviceState::Error || ds.state == DeviceState::Exiting) {
                ++fCount;
            }
            // but always list at as failed/outstanding
            fOutstandingDevices.emplace(task.GetId());
        }
    }

    void Update(const DDSTask::Id taskId, cc::Result result)
    {
        std::lock_guard<std::mutex> lk(fMtx);
        if (result == cc::Result::Ok) {
            fOutstandingDevices.erase(taskId);
        }
        ++fCount;
        TryCompletion();
    }

    /// precondition: fMtx is locked.
    void TryCompletion()
    {
        if (!fOp.IsCompleted() && fCount == fTasks.size()) {
            fTimer.cancel();
            if (!fOutstandingDevices.empty()) {
                fOp.Complete(MakeErrorCode(ErrorCode::DeviceSetPropertiesFailed), fOutstandingDevices);
            } else {
                fOp.Complete(fOutstandingDevices);
            }
        }
    }

    bool IsCompleted() { return fOp.IsCompleted(); }

  private:
    uint64_t const fId;
    AsioAsyncOp<Executor, Allocator, SetPropertiesCompletionSignature> fOp;
    boost::asio::steady_timer fTimer;
    unsigned int fCount;
    std::vector<DDSTask> fTasks;
    FailedDevices fOutstandingDevices;
    std::mutex& fMtx;

    /// precondition: fMtx is locked.
    bool ContainsTask(DDSTask::Id id)
    {
        auto it = std::find_if(fTasks.begin(), fTasks.end(), [id](const DDSTask& t) { return t.GetId() == id; });
        return it != fTasks.end();
    }
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPSETPROPERTIES */
