// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#include "Error.h"

using namespace std;

namespace odc::core
{
    const char* ErrorCategory::name() const noexcept
    {
        return "odc";
    }

    std::string ErrorCategory::message(int _condition) const
    {
        switch (static_cast<ErrorCode>(_condition))
        {
            case ErrorCode::RequestNotSupported:
                return "Request not supported";
            case ErrorCode::RequestTimeout:
                return "Request timeout";
            case ErrorCode::ResourcePluginFailed:
                return "Resource plugin failed";
            case ErrorCode::DDSCreateSessionFailed:
                return "Failed to create a DDS session";
            case ErrorCode::DDSShutdownSessionFailed:
                return "Failed to shutdown a DDS session";
            case ErrorCode::DDSAttachToSessionFailed:
                return "Failed to attach to a DDS session";
            case ErrorCode::DDSCreateTopologyFailed:
                return "Failed to create DDS topology";
            case ErrorCode::DDSCommanderInfoFailed:
                return "Failed to receive DDS commander info";
            case ErrorCode::DDSSubmitAgentsFailed:
                return "Failed to submit DDS agents";
            case ErrorCode::DDSActivateTopologyFailed:
                return "Failed to activate DDS topology";
            case ErrorCode::FairMQCreateTopologyFailed:
                return "Failed to create FairMQ topology";
            case ErrorCode::FairMQChangeStateFailed:
                return "Failed to change FairMQ device state";
            case ErrorCode::FairMQGetStateFailed:
                return "Failed to get FairMQ device state";
            case ErrorCode::FairMQSetPropertiesFailed:
                return "Failed to set FairMQ device properties";
            default:
                return "Unknown error";
        }
    }

    const ErrorCategory errorCategory{};

    error_code MakeErrorCode(ErrorCode _e)
    {
        return { static_cast<int>(_e), errorCategory };
    }
} // namespace odc::core
