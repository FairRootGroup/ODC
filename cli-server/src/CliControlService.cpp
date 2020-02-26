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

std::string CCliControlService::requestUpscale(const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_params));
}

std::string CCliControlService::requestDownscale(const odc::core::SUpdateParams& _params)
{
    return generalReply(m_service->execUpdate(_params));
}

std::string CCliControlService::requestConfigure()
{
    return generalReply(m_service->execConfigure());
}

std::string CCliControlService::requestStart()
{
    return generalReply(m_service->execStart());
}

std::string CCliControlService::requestStop()
{
    return generalReply(m_service->execStop());
}

std::string CCliControlService::requestReset()
{
    return generalReply(m_service->execReset());
}

std::string CCliControlService::requestTerminate()
{
    return generalReply(m_service->execTerminate());
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
