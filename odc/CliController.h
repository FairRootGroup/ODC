/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CLI_CONTROLLER
#define ODC_CLI_CONTROLLER

#include <odc/CliServiceHelper.h>
#include <odc/Controller.h>
#include <odc/DDSSubmit.h>

#include <chrono>
#include <sstream>

namespace odc::cli
{
class Controller : public core::CliServiceHelper<Controller>
{
  public:
    Controller() {}

    void setTimeout(const std::chrono::seconds& timeout) { mCtrl.setTimeout(timeout); }

    void registerResourcePlugins(const core::CPluginManager::PluginMap_t& pluginMap) { mCtrl.registerResourcePlugins(pluginMap); }
    void registerRequestTriggers(const core::CPluginManager::PluginMap_t& triggerMap) { mCtrl.registerRequestTriggers(triggerMap); }
    void restore(const std::string& restoreId) { mCtrl.restore(restoreId); }

    std::string requestInitialize(const core::CommonParams& common, const core::InitializeParams& params) { return generalReply(mCtrl.execInitialize(common, params)); }
    std::string requestSubmit(const core::CommonParams& common, const core::SubmitParams& params) { return generalReply(mCtrl.execSubmit(common, params)); }
    std::string requestActivate(const core::CommonParams& common, const core::ActivateParams& params) { return generalReply(mCtrl.execActivate(common, params)); }
    std::string requestRun(const core::CommonParams& common,
                           const core::InitializeParams& initializeParams,
                           const core::SubmitParams& submitParams,
                           const core::ActivateParams& activateParams)
    {
        return generalReply(mCtrl.execRun(common, initializeParams, submitParams, activateParams));
    }

    std::string requestUpscale(const core::CommonParams& common, const core::UpdateParams& params) { return generalReply(mCtrl.execUpdate(common, params)); }
    std::string requestDownscale(const core::CommonParams& common, const core::UpdateParams& params) { return generalReply(mCtrl.execUpdate(common, params)); }
    std::string requestGetState(const core::CommonParams& common, const core::DeviceParams& params) { return generalReply(mCtrl.execGetState(common, params)); }
    std::string requestSetProperties(const core::CommonParams& common, const core::SetPropertiesParams& params)
    {
        return generalReply(mCtrl.execSetProperties(common, params));
    }
    std::string requestConfigure(const core::CommonParams& common, const core::DeviceParams& params) { return generalReply(mCtrl.execConfigure(common, params)); }
    std::string requestStart(const core::CommonParams& common, const core::DeviceParams& params) { return generalReply(mCtrl.execStart(common, params)); }

    std::string requestStop(const core::CommonParams& common, const core::DeviceParams& params) { return generalReply(mCtrl.execStop(common, params)); }
    std::string requestReset(const core::CommonParams& common, const core::DeviceParams& params) { return generalReply(mCtrl.execReset(common, params)); }
    std::string requestTerminate(const core::CommonParams& common, const core::DeviceParams& params) { return generalReply(mCtrl.execTerminate(common, params)); }
    std::string requestShutdown(const core::CommonParams& common) { return generalReply(mCtrl.execShutdown(common)); }
    std::string requestStatus(const core::StatusParams& params) { return statusReply(mCtrl.execStatus(params)); }

  private:
    std::string generalReply(const core::RequestResult& result)
    {
        std::stringstream ss;

        if (result.m_statusCode == core::StatusCode::ok) {
            ss << "  Status code: SUCCESS\n  Message: " << result.m_msg << std::endl;
        } else {
            ss << "  Status code: ERROR\n  Error code: " << result.m_error.m_code.value()
               << "\n  Error message: " << result.m_error.m_code.message() << " ("
               << result.m_error.m_details << ")" << std::endl;
        }

        ss << "  Aggregated state: " << result.mAggregatedState << std::endl;
        ss << "  Partition ID: " << result.mPartitionID << std::endl;
        ss << "  Run Nr: " << result.mRunNr << std::endl;
        ss << "  Session ID: " << result.mDDSSessionID << std::endl;

        if (result.mFullState != nullptr) {
            ss << std::endl << "  Devices: " << std::endl;
            for (const auto& state : *(result.mFullState)) {
                ss << "    { id: " << state.m_status.taskId << "; path: " << state.m_path << "; state: " << state.m_status.state << " }" << std::endl;
            }
            ss << std::endl;
        }

        ss << "  Execution time: " << result.m_execTime << " msec" << std::endl;

        return ss.str();
    }

    std::string statusReply(const core::StatusRequestResult& result)
    {
        std::stringstream ss;
        if (result.m_statusCode == core::StatusCode::ok) {
            ss << "  Status code: SUCCESS\n  Message: " << result.m_msg << std::endl;
        } else {
            ss << "  Status code: ERROR\n  Error code: " << result.m_error.m_code.value() << "\n  Error message: " << result.m_error.m_code.message() << " ("
               << result.m_error.m_details << ")" << std::endl;
        }
        ss << "  Partitions: " << std::endl;
        for (const auto& p : result.m_partitions) {
            ss << "    { partition ID: " << p.mPartitionID << "; session ID: " << p.mDDSSessionID
               << "; status: " << ((p.mDDSSessionStatus == core::DDSSessionStatus::running) ? "RUNNING" : "STOPPED")
               << "; state: " << core::GetAggregatedStateName(p.mAggregatedState) << " }" << std::endl;
        }
        ss << "  Execution time: " << result.m_execTime << " msec" << std::endl;
        return ss.str();
    }

  private:
    core::Controller mCtrl; ///< Core ODC service
};
} // namespace odc::cli

#endif /* defined(ODC_CLI_CONTROLLER) */
