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

using ChangeStateCompletionSignature = void(std::error_code, TopologyState);

template<typename Executor, typename Allocator>
struct ChangeStateOp
{
    template<typename Handler>
    ChangeStateOp(uint64_t id,
                  const TopologyTransition transition,
                  std::vector<DDSTask> tasks,
                  TopologyState& stateData,
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
    auto ResetCount(const TopologyStateIndex& stateIndex, const TopologyState& stateData) -> void
    {
        fCount = std::count_if(stateIndex.cbegin(), stateIndex.cend(), [=](const auto& s) {
            if (ContainsTask(stateData.at(s.second).taskId)) {
                return stateData.at(s.second).state == fTargetState;
            } else {
                return false;
            }
        });
    }

    /// precondition: fMtx is locked.
    auto Update(const DDSTask::Id taskId, const DeviceState currentState) -> void
    {
        if (!fOp.IsCompleted() && ContainsTask(taskId)) {
            if (currentState == fTargetState) {
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
    auto TryCompletion() -> void
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
    auto Complete(std::error_code ec) -> void
    {
        fTimer.cancel();
        fOp.Complete(ec, fStateData);
    }

    /// precondition: fMtx is locked.
    auto ContainsTask(DDSTask::Id id) -> bool
    {
        auto it = std::find_if(fTasks.begin(), fTasks.end(), [id](const DDSTask& t) { return t.GetId() == id; });
        return it != fTasks.end();
    }

    bool IsCompleted() { return fOp.IsCompleted(); }

    auto GetTargetState() const -> DeviceState { return fTargetState; }

  private:
    const uint64_t fId;
    AsioAsyncOp<Executor, Allocator, ChangeStateCompletionSignature> fOp;
    TopologyState& fStateData;
    boost::asio::steady_timer fTimer;
    unsigned int fCount;
    std::vector<DDSTask> fTasks;
    DeviceState fTargetState;
    std::mutex& fMtx;
    bool fErrored;
};

} // namespace odc::core

#endif /* ODC_TOPOLOGYOPCHANGESTATE */
