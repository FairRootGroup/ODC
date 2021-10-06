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

void CCliControlService::registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap)
{
    m_service->registerResourcePlugins(_pluginMap);
}

void CCliControlService::registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap)
{
    m_service->registerRequestTriggers(_triggerMap);
}

void CCliControlService::restore(const std::string& _restoreId)
{
    m_service->restore(_restoreId);
}

std::string CCliControlService::requestInitialize(const partitionID_t& _partitionID, const SInitializeParams& _params)
{
    return generalReply(m_service->execInitialize(_partitionID, _params));
}

std::string CCliControlService::requestSubmit(const partitionID_t& _partitionID, const SSubmitParams& _params)
{
    return generalReply(m_service->execSubmit(_partitionID, _params));
}

std::string CCliControlService::requestActivate(const partitionID_t& _partitionID, const SActivateParams& _params)
{
    return generalReply(m_service->execActivate(_partitionID, _params));
}

std::string CCliControlService::requestRun(const partitionID_t& _partitionID,
                                           const SInitializeParams& _initializeParams,
                                           const SSubmitParams& _submitParams,
                                           const SActivateParams& _activateParams)
{
    return generalReply(m_service->execRun(_partitionID, _initializeParams, _submitParams, _activateParams));
}

std::string CCliControlService::requestUpscale(const partitionID_t& _partitionID, const SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_partitionID, _params));
}

std::string CCliControlService::requestDownscale(const partitionID_t& _partitionID, const SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_partitionID, _params));
}

std::string CCliControlService::requestGetState(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return generalReply(m_service->execGetState(_partitionID, _params));
}

std::string CCliControlService::requestSetProperties(const partitionID_t& _partitionID,
                                                     const SSetPropertiesParams& _params)
{
    return generalReply(m_service->execSetProperties(_partitionID, _params));
}

std::string CCliControlService::requestConfigure(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return generalReply(m_service->execConfigure(_partitionID, _params));
}

std::string CCliControlService::requestStart(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return generalReply(m_service->execStart(_partitionID, _params));
}

std::string CCliControlService::requestStop(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return generalReply(m_service->execStop(_partitionID, _params));
}

std::string CCliControlService::requestReset(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return generalReply(m_service->execReset(_partitionID, _params));
}

std::string CCliControlService::requestTerminate(const partitionID_t& _partitionID, const SDeviceParams& _params)
{
    return generalReply(m_service->execTerminate(_partitionID, _params));
}

std::string CCliControlService::requestShutdown(const partitionID_t& _partitionID)
{
    return generalReply(m_service->execShutdown(_partitionID));
}

std::string CCliControlService::requestStatus(const SStatusParams& _params)
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

std::string CCliControlService::statusReply(const SStatusReturnValue& _value)
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
