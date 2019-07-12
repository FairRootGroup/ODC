// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// DDS
#include "DDS/dds_intercom.h"
// STD
#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace dds;
using namespace dds::intercom_api;

int main(int argc, char** argv)
{

    // DDS custom command API
    CIntercomService service;
    CCustomCmd customCmd(service);

    // Best practice is to subscribe on errors first, before doing any other function calls.
    // Otherwise there is a chance to miss some of the error messages from DDS.
    service.subscribeOnError([](const EErrorCode _errorCode, const string& _errorMsg) {
        cout << "Error received: error code: " << _errorCode << ", error message: " << _errorMsg << endl;
    });

    // Subscribe on custom commands
    customCmd.subscribe([&customCmd](const string& _command, const string& _condition, uint64_t _senderId) {
        cout << "Received custom command: " << _command << " condition: " << _condition << " senderId: " << _senderId
             << endl;
        string senderIdStr = to_string(_senderId);
        customCmd.send(_command + "_" + senderIdStr, senderIdStr);
    });

    // Subscribe on reply from DDS commander server
    customCmd.subscribeOnReply([](const string& _msg) { cout << "Received reply message: " << _msg << endl; });

    service.start();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return EXIT_SUCCESS;
}
