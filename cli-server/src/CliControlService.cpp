// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "CliControlService.h"

using namespace odc;
using namespace odc::core;
using namespace odc::cli;
using namespace std;

CCliControlService::CCliControlService()
    : m_service(make_shared<CControlService>())
{
}

void CCliControlService::Initialize(const string& _rmsPlugin, const string& _configFile, const string& _topologyFile)
{
    SInitializeParams params{ _topologyFile, _rmsPlugin, _configFile };
    SReturnValue value = m_service->Initialize(params);
    printGeneralReply(value);
}

void CCliControlService::ConfigureRun()
{
    SReturnValue value = m_service->ConfigureRun();
    printGeneralReply(value);
}

void CCliControlService::Start()
{
    SReturnValue value = m_service->Start();
    printGeneralReply(value);
}

void CCliControlService::Stop()
{
    SReturnValue value = m_service->Stop();
    printGeneralReply(value);
}

void CCliControlService::Terminate()
{
    SReturnValue value = m_service->Terminate();
    printGeneralReply(value);
}

void CCliControlService::Shutdown()
{
    SReturnValue value = m_service->Shutdown();
    printGeneralReply(value);
}

 void CCliControlService::UpdateTopology(const std::string& _topologyFile)
{
    SUpdateTopologyParams params{ _topologyFile };
    SReturnValue value = m_service->UpdateTopology(params);
    printGeneralReply(value);
}

void CCliControlService::printGeneralReply(const SReturnValue& _value)
{
    if (_value.m_statusCode == EStatusCode::ok)
    {
        cout << "Status code: SUCCESS. Message: " << _value.m_msg << endl;
    }
    else
    {
        cerr << "Status code: ERROR. Error code: " << _value.m_error.m_code
             << ". Error message: " << _value.m_error.m_msg << endl;
    }
    cout << "Execution time: " << _value.m_execTime << " msec" << endl;
}
