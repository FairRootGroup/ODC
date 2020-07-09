// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "CliControlService.h"
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

std::string CCliControlService::requestInitialize(const odc::core::SInitializeParams& _params)
{
    return generalReply(m_service->execInitialize(_params));
}

std::string CCliControlService::requestSubmit(const odc::core::SSubmitParams& _params)
{
    return generalReply(m_service->execSubmit(_params));
}

std::string CCliControlService::requestActivate(const odc::core::SActivateParams& _params)
{
    return generalReply(m_service->execActivate(_params));
}

std::string CCliControlService::requestRun(const odc::core::SInitializeParams& _initializeParams,
                                           const odc::core::SSubmitParams& _submitParams,
                                           const odc::core::SActivateParams& _activateParams)
{
    return generalReply(m_service->execRun(_initializeParams, _submitParams, _activateParams));
}

std::string CCliControlService::requestUpscale(const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_params));
}

std::string CCliControlService::requestDownscale(const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_params));
}

std::string CCliControlService::requestGetState(const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execGetState(_params));
}

std::string CCliControlService::requestConfigure(const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execConfigure(_params));
}

std::string CCliControlService::requestStart(const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execStart(_params));
}

std::string CCliControlService::requestStop(const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execStop(_params));
}

std::string CCliControlService::requestReset(const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execReset(_params));
}

std::string CCliControlService::requestTerminate(const odc::core::SDeviceParams& _params)
{
    return generalReply(m_service->execTerminate(_params));
}

std::string CCliControlService::requestShutdown()
{
    return generalReply(m_service->execShutdown());
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
        ss << "  Status code: ERROR\n  Error code: " << _value.m_error.m_code
           << "\n  Error message: " << _value.m_error.m_msg << endl;
    }

    ss << "  Aggregated state: " << _value.m_aggregatedState << endl;
    ss << "  Run ID: " << _value.m_runID << endl;
    ss << "  Session ID: " << _value.m_sessionID << endl;

    if (_value.m_details != nullptr)
    {
        ss << endl << "  Devices: " << endl;
        const auto& topologyState = _value.m_details->m_topologyState;
        for (const auto& state : topologyState)
        {
            ss << "    { id: " << state.m_status.taskId << "; path: " << state.m_path
               << "; state: " << fair::mq::GetStateName(state.m_status.state) << " }" << endl;
        }
        ss << endl;
    }

    ss << "  Execution time: " << _value.m_execTime << " msec" << endl;

    return ss.str();
}
