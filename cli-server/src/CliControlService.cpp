// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "CliControlService.h"
#include "Topology.h"
// STD
#include <sstream>

using namespace odc;
using namespace odc::core;
using namespace odc::cli;
using namespace std;

CCliControlService::CCliControlService()
    : m_service(make_shared<CControlService>())
{
}

void CCliControlService::setTimeout(const std::chrono::seconds& _timeout)
{
    m_service->setTimeout(_timeout);
}

void CCliControlService::registerResourcePlugins(const odc::core::CDDSSubmit::PluginMap_t& _pluginMap)
{
    m_service->registerResourcePlugins(_pluginMap);
}

std::string CCliControlService::requestInitialize(const odc::core::partitionID_t& _partitionID,
                                                  const odc::core::SInitializeParams& _params)
{
    return generalReply(m_service->execInitialize(_partitionID, _params));
}

std::string CCliControlService::requestSubmit(const odc::core::partitionID_t& _partitionID,
                                              const odc::core::SSubmitParams& _params)
{
    return generalReply(m_service->execSubmit(_partitionID, _params));
}

std::string CCliControlService::requestActivate(const odc::core::partitionID_t& _partitionID,
                                                const odc::core::SActivateParams& _params)
{
    return generalReply(m_service->execActivate(_partitionID, _params));
}

std::string CCliControlService::requestRun(const odc::core::partitionID_t& _partitionID,
                                           const odc::core::SInitializeParams& _initializeParams,
                                           const odc::core::SSubmitParams& _submitParams,
                                           const odc::core::SActivateParams& _activateParams)
{
    return generalReply(m_service->execRun(_partitionID, _initializeParams, _submitParams, _activateParams));
}

std::string CCliControlService::requestUpscale(const odc::core::partitionID_t& _partitionID,
                                               const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_partitionID, _params));
}

std::string CCliControlService::requestDownscale(const odc::core::partitionID_t& _partitionID,
                                                 const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_partitionID, _params));
}

std::string CCliControlService::requestGetState(const odc::core::partitionID_t& _partitionID,
                                                const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execGetState(_partitionID, _params));
}

std::string CCliControlService::requestSetProperties(const odc::core::partitionID_t& _partitionID,
                                                     const odc::core::SSetPropertiesParams& _params)
{
    return generalReply(m_service->execSetProperties(_partitionID, _params));
}

std::string CCliControlService::requestConfigure(const odc::core::partitionID_t& _partitionID,
                                                 const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execConfigure(_partitionID, _params));
}

std::string CCliControlService::requestStart(const odc::core::partitionID_t& _partitionID,
                                             const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execStart(_partitionID, _params));
}

std::string CCliControlService::requestStop(const odc::core::partitionID_t& _partitionID,
                                            const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execStop(_partitionID, _params));
}

std::string CCliControlService::requestReset(const odc::core::partitionID_t& _partitionID,
                                             const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execReset(_partitionID, _params));
}

std::string CCliControlService::requestTerminate(const odc::core::partitionID_t& _partitionID,
                                                 const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execTerminate(_partitionID, _params));
}

std::string CCliControlService::requestShutdown(const odc::core::partitionID_t& _partitionID)
{
    return generalReply(m_service->execShutdown(_partitionID));
}

std::string CCliControlService::requestStatus(const odc::core::SStatusParams& _params)
{
    return statusReply(m_service->execStatus(_params));
}

string CCliControlService::generalReply(const SReturnValue& _value)
{
    stringstream ss;

    if (_value.m_statusCode == EStatusCode::ok)
    {
        ss << "  Status code: SUCCESS\n  Message: " << _value.m_msg << endl;
    }
    else
    {
        ss << "  Status code: ERROR\n  Error code: " << _value.m_error.m_code.value()
           << "\n  Error message: " << _value.m_error.m_code.message() << " (" << _value.m_error.m_details << ")"
           << endl;
    }

    ss << "  Aggregated state: " << _value.m_aggregatedState << endl;
    ss << "  Partition ID: " << _value.m_partitionID << endl;
    ss << "  Session ID: " << _value.m_sessionID << endl;

    if (_value.m_details != nullptr)
    {
        ss << endl << "  Devices: " << endl;
        const auto& topologyState = _value.m_details->m_topologyState;
        for (const auto& state : topologyState)
        {
            ss << "    { id: " << state.m_status.taskId << "; path: " << state.m_path
               << "; state: " << state.m_status.state << " }" << endl;
        }
        ss << endl;
    }

    ss << "  Execution time: " << _value.m_execTime << " msec" << endl;

    return ss.str();
}

std::string CCliControlService::statusReply(const odc::core::SStatusReturnValue& _value)
{
    stringstream ss;
    if (_value.m_statusCode == EStatusCode::ok)
    {
        ss << "  Status code: SUCCESS\n  Message: " << _value.m_msg << endl;
    }
    else
    {
        ss << "  Status code: ERROR\n  Error code: " << _value.m_error.m_code.value()
           << "\n  Error message: " << _value.m_error.m_code.message() << " (" << _value.m_error.m_details << ")"
           << endl;
    }
    ss << "  Partitions: " << endl;
    for (const auto& p : _value.m_partitions)
    {
        ss << "    { partition ID: " << p.m_partitionID << "; session ID: " << p.m_sessionID
           << "; status: " << ((p.m_sessionStatus == ESessionStatus::running) ? "RUNNING" : "STOPPED")
           << "; state: " << GetAggregatedTopologyStateName(p.m_aggregatedState) << " }" << endl;
    }
    ss << "  Execution time: " << _value.m_execTime << " msec" << endl;
    return ss.str();
}
