/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CLICONTROLLER
#define ODC_CLICONTROLLER

#include <odc/CliControllerHelper.h>
#include <odc/Controller.h>
#include <odc/DDSSubmit.h>

#include <chrono>
#include <sstream>

namespace odc {

class CliController : public core::CliControllerHelper<CliController>
{
  public:
    CliController() {}

    void setTimeout(const std::chrono::seconds& timeout) { mCtrl.setTimeout(timeout); }
    void setHistoryDir(const std::string& dir) { mCtrl.setHistoryDir(dir); }

    void registerResourcePlugins(const core::PluginManager::PluginMap& pluginMap) { mCtrl.registerResourcePlugins(pluginMap); }
    void registerRequestTriggers(const core::PluginManager::PluginMap& triggerMap) { mCtrl.registerRequestTriggers(triggerMap); }
    void restore(const std::string& restoreId, const std::string& restoreDir) { mCtrl.restore(restoreId, restoreDir); }

    std::string requestInitialize(   const core::CommonParams& common, const core::InitializeParams& params)    { return generalReply(mCtrl.execInitialize(common, params)); }
    std::string requestSubmit(       const core::CommonParams& common, const core::SubmitParams& params)        { return generalReply(mCtrl.execSubmit(common, params)); }
    std::string requestActivate(     const core::CommonParams& common, const core::ActivateParams& params)      { return generalReply(mCtrl.execActivate(common, params)); }
    std::string requestRun(          const core::CommonParams& common, const core::RunParams& params)           { return generalReply(mCtrl.execRun(common, params)); }
    std::string requestUpscale(      const core::CommonParams& common, const core::UpdateParams& params)        { return generalReply(mCtrl.execUpdate(common, params)); }
    std::string requestDownscale(    const core::CommonParams& common, const core::UpdateParams& params)        { return generalReply(mCtrl.execUpdate(common, params)); }
    std::string requestGetState(     const core::CommonParams& common, const core::DeviceParams& params)        { return generalReply(mCtrl.execGetState(common, params)); }
    std::string requestSetProperties(const core::CommonParams& common, const core::SetPropertiesParams& params) { return generalReply(mCtrl.execSetProperties(common, params)); }
    std::string requestConfigure(    const core::CommonParams& common, const core::DeviceParams& params)        { return generalReply(mCtrl.execConfigure(common, params)); }
    std::string requestStart(        const core::CommonParams& common, const core::DeviceParams& params)        { return generalReply(mCtrl.execStart(common, params)); }
    std::string requestStop(         const core::CommonParams& common, const core::DeviceParams& params)        { return generalReply(mCtrl.execStop(common, params)); }
    std::string requestReset(        const core::CommonParams& common, const core::DeviceParams& params)        { return generalReply(mCtrl.execReset(common, params)); }
    std::string requestTerminate(    const core::CommonParams& common, const core::DeviceParams& params)        { return generalReply(mCtrl.execTerminate(common, params)); }
    std::string requestShutdown(     const core::CommonParams& common)                                          { return generalReply(mCtrl.execShutdown(common)); }
    std::string requestStatus(       const core::StatusParams& params)                                          { return statusReply(mCtrl.execStatus(params)); }

  private:
    std::string generalReply(const core::RequestResult& result)
    {
        std::stringstream ss;

        if (result.mStatusCode == core::StatusCode::ok) {
            ss << "  Status code: SUCCESS\n  Message: " << result.mMsg << std::endl;
        } else {
            ss << "  Status code: ERROR\n  Error code: " << result.mError.mCode.value()
               << "\n  Error message: " << result.mError.mCode.message() << " ("
               << result.mError.mDetails << ")" << std::endl;
        }

        ss << "  Aggregated state: " << result.mAggregatedState << std::endl;
        ss << "  Partition ID: " << result.mPartitionID << std::endl;
        ss << "  Run Nr: " << result.mRunNr << std::endl;
        ss << "  Session ID: " << result.mDDSSessionID << std::endl;

        if (result.mDetailedState != nullptr) {
            ss << std::endl << "  Devices: " << std::endl;
            for (const auto& state : *(result.mDetailedState)) {
                ss << "    { id: " << state.mStatus.taskId
                       << ", path: " << state.mPath
                       << ", state: " << state.mStatus.state
                       << ", ignored: " <<  state.mStatus.ignored
                       << ", host: " <<  state.mHost
                       << " }" << std::endl;
            }
            ss << std::endl;
        }

        ss << "  Execution time: " << result.mExecTime << " msec" << std::endl;

        return ss.str();
    }

    std::string statusReply(const core::StatusRequestResult& result)
    {
        std::stringstream ss;
        if (result.mStatusCode == core::StatusCode::ok) {
            ss << "  Status code: SUCCESS\n  Message: " << result.mMsg << std::endl;
        } else {
            ss << "  Status code: ERROR\n  Error code: " << result.mError.mCode.value() << "\n  Error message: " << result.mError.mCode.message() << " ("
               << result.mError.mDetails << ")" << std::endl;
        }
        ss << "  Partitions: " << std::endl;
        for (const auto& p : result.mPartitions) {
            ss << "    { partition ID: " << p.mPartitionID << "; session ID: " << p.mDDSSessionID
               << "; status: " << ((p.mDDSSessionStatus == core::DDSSessionStatus::running) ? "RUNNING" : "STOPPED")
               << "; state: " << core::GetAggregatedStateName(p.mAggregatedState) << " }" << std::endl;
        }
        ss << "  Execution time: " << result.mExecTime << " msec" << std::endl;
        return ss.str();
    }

  private:
    core::Controller mCtrl; ///< Core ODC service
};
} // namespace odc::cli

#endif /* defined(ODC_CLICONTROLLER) */
