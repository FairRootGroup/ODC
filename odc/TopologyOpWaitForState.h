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
        : fId(id)
        , fOp(ex, alloc, std::move(handler))
        , fTimer(ex)
        , fCount(0)
        , fTasks(std::move(tasks))
        , fTargetLastState(targetLastState)
        , fTargetCurrentState(targetCurrentState)
        , fMtx(mutex)
        , fErrored(false)
    {
        if (timeout > std::chrono::milliseconds(0)) {
            fTimer.expires_after(timeout);
            fTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    std::lock_guard<std::mutex> lk(fMtx);
                    fOp.Timeout();
                }
            });
        }
        if (fTasks.empty()) {
            OLOG(warning) << "WaitForState initiated on an empty set of tasks, check the path argument.";
        }
    }
    WaitForStateOp() = delete;
    WaitForStateOp(const WaitForStateOp&) = delete;
    WaitForStateOp& operator=(const WaitForStateOp&) = delete;
    WaitForStateOp(WaitForStateOp&&) = default;
    WaitForStateOp& operator=(WaitForStateOp&&) = default;
    ~WaitForStateOp() = default;

    /// precondition: fMtx is locked.
    void ResetCount(const TopologyStateIndex& stateIndex, const TopologyState& stateData)
    {
        fCount = std::count_if(stateIndex.cbegin(), stateIndex.cend(), [=](const auto& s) {
            if (ContainsTask(stateData.at(s.second).taskId)) {
                return stateData.at(s.second).state == fTargetCurrentState
                       && (stateData.at(s.second).lastState == fTargetLastState || fTargetLastState == DeviceState::Undefined);
            } else {
                return false;
            }
        });
    }

    /// precondition: fMtx is locked.
    void Update(const DDSTask::Id taskId, const DeviceState lastState, const DeviceState currentState)
    {
        if (!fOp.IsCompleted() && ContainsTask(taskId)) {
            if (currentState == fTargetCurrentState && (lastState == fTargetLastState || fTargetLastState == DeviceState::Undefined)) {
                ++fCount;
            }
            if (currentState == DeviceState::Error) {
                fErrored = true;
                ++fCount;
            }
            TryCompletion();
        }
    }

    /// precondition: fMtx is locked.
    void TryCompletion()
    {
        if (!fOp.IsCompleted() && fCount == fTasks.size()) {
            fTimer.cancel();
            if (fErrored) {
                fOp.Complete(MakeErrorCode(ErrorCode::DeviceChangeStateFailed));
            } else {
                fOp.Complete();
            }
        }
    }

    bool IsCompleted() { return fOp.IsCompleted(); }

  private:
    const uint64_t fId;
    AsioAsyncOp<Executor, Allocator, WaitForStateCompletionSignature> fOp;
    boost::asio::steady_timer fTimer;
    unsigned int fCount;
    std::vector<DDSTask> fTasks;
    DeviceState fTargetLastState;
    DeviceState fTargetCurrentState;
    std::mutex& fMtx;
    bool fErrored;

    /// precondition: fMtx is locked.
    bool ContainsTask(DDSTask::Id id)
    {
        auto it = std::find_if(fTasks.begin(), fTasks.end(), [id](const DDSTask& t) { return t.GetId() == id; });
        return it != fTasks.end();
    }
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPWAITFORSTATE */
