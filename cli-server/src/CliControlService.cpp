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

void CCliControlService::setRecoTopoPath(const std::string& _path)
{
    m_recoTopoPath = _path;
}

void CCliControlService::setQCTopoPath(const std::string& _path)
{
    m_qcTopoPath = _path;
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

std::string CCliControlService::requestUpscale(const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_params));
}

std::string CCliControlService::requestDownscale(const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_params));
}

std::string CCliControlService::requestConfigure(hlp::EDeviceType _deviceType)
{
    return generalReply(m_service->execConfigure(pathForDevice(_deviceType)));
}

std::string CCliControlService::requestStart(hlp::EDeviceType _deviceType)
{
    return generalReply(m_service->execStart(pathForDevice(_deviceType)));
}

std::string CCliControlService::requestStop(hlp::EDeviceType _deviceType)
{
    return generalReply(m_service->execStop(pathForDevice(_deviceType)));
}

std::string CCliControlService::requestReset(hlp::EDeviceType _deviceType)
{
    return generalReply(m_service->execReset(pathForDevice(_deviceType)));
}

std::string CCliControlService::requestTerminate(hlp::EDeviceType _deviceType)
{
    return generalReply(m_service->execTerminate(pathForDevice(_deviceType)));
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
        ss << "Status code: SUCCESS. Message: " << _value.m_msg << endl;
    }
    else
    {
        ss << "Status code: ERROR. Error code: " << _value.m_error.m_code << ". Error message: " << _value.m_error.m_msg
           << endl;
    }
    ss << "Execution time: " << _value.m_execTime << " msec";

    return ss.str();
}

string CCliControlService::pathForDevice(hlp::EDeviceType _type)
{
    switch (_type)
    {
        case hlp::EDeviceType::reco:
            return m_recoTopoPath;

        case hlp::EDeviceType::qc:
            return m_qcTopoPath;

        default:
            return "";
    }
    return "";
}
