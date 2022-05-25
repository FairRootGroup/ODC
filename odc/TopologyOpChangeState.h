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
#include <vector>

namespace odc::core
{

using ChangeStateCompletionSignature = void(std::error_code, TopoState);

template<typename Executor, typename Allocator>
struct ChangeStateOp
{
    template<typename Handler>
    ChangeStateOp(uint64_t id,
                  TopoTransition transition,
                  std::vector<DDSTask> tasks,
                  TopoState& stateData,
                  Duration timeout,
                  std::mutex& mutex,
                  Executor const& ex,
                  Allocator const& alloc,
                  Handler&& handler)
        : fId(id)
        , fOp(ex, alloc, std::move(handler))
        , fStateData(stateData)
        , fTimer(ex)
        , fCount(0)
        , fTasks(std::move(tasks))
        , fTargetState(gExpectedState.at(transition))
        , fMtx(mutex)
        , fErrored(false)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            fTimer.expires_after(timeout);
            fTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(fMtx);
                    fOp.Timeout(fStateData);
                }
            });
        }
        if (fTasks.empty()) {
            OLOG(warning) << "ChangeState initiated on an empty set of tasks, check the path argument.";
        }
    }
    ChangeStateOp() = delete;
    ChangeStateOp(const ChangeStateOp&) = delete;
    ChangeStateOp& operator=(const ChangeStateOp&) = delete;
    ChangeStateOp(ChangeStateOp&&) = default;
    ChangeStateOp& operator=(ChangeStateOp&&) = default;
    ~ChangeStateOp() = default;

    /// precondition: fMtx is locked.
    void ResetCount(const TopoStateIndex& stateIndex, const TopoState& stateData)
    {
        fCount = std::count_if(stateIndex.cbegin(), stateIndex.cend(), [=](const auto& s) {
            const auto& task = stateData.at(s.second);
            if (ContainsTask(task.taskId)) {
                // Do not wait for an errored device that is not yet ignored
                if (task.state == DeviceState::Error) {
                    fErrored = true;
                    return true;
                }
                return task.state == fTargetState;
            } else {
                return false;
            }
        });
    }

    /// precondition: fMtx is locked.
    void Update(const DDSTask::Id taskId, const DeviceState currentState)
    {
        if (!fOp.IsCompleted() && ContainsTask(taskId)) {
            if (currentState == fTargetState) {
                ++fCount;
            } else if (currentState == DeviceState::Error) {
                if (failed.count(taskId) == 0) {
                    fErrored = true;
                    ++fCount;
                    failed.emplace(taskId);
                } else {
                    // OLOG(debug) << "Task " << taskId << " is already in the set of failed devices";
                }
            }
            TryCompletion();
        }
    }

    /// precondition: fMtx is locked.
    void TryCompletion()
    {
        if (!fOp.IsCompleted() && fCount == fTasks.size()) {
            if (fErrored) {
                Complete(MakeErrorCode(ErrorCode::DeviceChangeStateFailed));
            } else {
                Complete(std::error_code());
            }
        }
    }

    /// precondition: fMtx is locked.
    void Complete(std::error_code ec)
    {
        fTimer.cancel();
        fOp.Complete(ec, fStateData);
    }

    /// precondition: fMtx is locked.
    bool ContainsTask(DDSTask::Id id)
    {
        auto it = std::find_if(fTasks.begin(), fTasks.end(), [id](const DDSTask& t) { return t.GetId() == id; });
        return it != fTasks.end();
    }

    bool IsCompleted() { return fOp.IsCompleted(); }

    DeviceState GetTargetState() const { return fTargetState; }

  private:
    const uint64_t fId;
    AsioAsyncOp<Executor, Allocator, ChangeStateCompletionSignature> fOp;
    TopoState& fStateData;
    boost::asio::steady_timer fTimer;
    unsigned int fCount;
    std::vector<DDSTask> fTasks;
    FailedDevices failed;
    DeviceState fTargetState;
    std::mutex& fMtx;
    bool fErrored;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPCHANGESTATE */
