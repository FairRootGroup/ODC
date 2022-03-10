/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <odc/Error.h>

using namespace std;

namespace odc::core
{
const char* ErrorCategory::name() const noexcept { return "odc"; }

std::string ErrorCategory::message(int condition) const
{
    switch (static_cast<ErrorCode>(condition)) {
        // clang-format off
        case ErrorCode::RequestNotSupported:        return "Request not supported";
        case ErrorCode::RequestTimeout:             return "Request timeout";
        case ErrorCode::ResourcePluginFailed:       return "Resource plugin failed";
        case ErrorCode::OperationInProgress:        return "async operation already in progress";
        case ErrorCode::OperationTimeout:           return "async operation timed out";
        case ErrorCode::OperationCanceled:          return "async operation canceled";
        case ErrorCode::DeviceChangeStateFailed:    return "Failed to change state of a FairMQ device";
        case ErrorCode::DeviceGetPropertiesFailed:  return "Failed to get FairMQ device properties";
        case ErrorCode::DeviceSetPropertiesFailed:  return "Failed to set FairMQ device properties";
        case ErrorCode::TopologyFailed:             return "Failed topology";
        case ErrorCode::DDSCreateSessionFailed:     return "Failed to create a DDS session";
        case ErrorCode::DDSShutdownSessionFailed:   return "Failed to shutdown a DDS session";
        case ErrorCode::DDSAttachToSessionFailed:   return "Failed to attach to a DDS session";
        case ErrorCode::DDSCreateTopologyFailed:    return "Failed to create DDS topology";
        case ErrorCode::DDSCommanderInfoFailed:     return "Failed to receive DDS commander info";
        case ErrorCode::DDSSubmitAgentsFailed:      return "Failed to submit DDS agents";
        case ErrorCode::DDSActivateTopologyFailed:  return "Failed to activate DDS topology";
        case ErrorCode::FairMQCreateTopologyFailed: return "Failed to create FairMQ topology";
        case ErrorCode::FairMQChangeStateFailed:    return "Failed to change FairMQ device state";
        case ErrorCode::FairMQGetStateFailed:       return "Failed to get FairMQ device state";
        case ErrorCode::FairMQSetPropertiesFailed:  return "Failed to set FairMQ device properties";
        default:                                    return "Unknown error";
        // clang-format on
    }
}

const ErrorCategory errorCategory {};

error_code MakeErrorCode(ErrorCode e) { return { static_cast<int>(e), errorCategory }; }
} // namespace odc::core
