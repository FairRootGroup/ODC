/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__Error__
#define __ODC__Error__

// ODC
#include <odc/MiscUtils.h>
// STD
#include <stdexcept>
#include <system_error>

namespace odc::core
{
struct RuntimeError : std::runtime_error
{
    template<typename... T>
    explicit RuntimeError(T&&... t)
        : std::runtime_error::runtime_error(toString(std::forward<T>(t)...))
    {}
};

struct Error
{
    Error() {}
    Error(std::error_code code, const std::string& details)
        : mCode(code)
        , mDetails(details)
    {}

    std::error_code mCode; ///< Error code
    std::string mDetails;  ///< Details of the error

    friend std::ostream& operator<<(std::ostream& os, const Error& error) { return os << "[" << error.mCode << "]: " << error.mDetails; }
};

enum class ErrorCode
{
    RequestNotSupported = 100,
    RequestTimeout,
    ResourcePluginFailed,
    OperationInProgress,
    OperationTimeout,
    OperationCanceled,
    DeviceChangeStateFailed,
    DeviceChangeStateInvalidTransition,
    DeviceGetPropertiesFailed,
    DeviceSetPropertiesFailed,
    DeviceWaitForStateFailed,
    TopologyFailed,

    DDSCreateSessionFailed = 200,
    DDSShutdownSessionFailed,
    DDSAttachToSessionFailed,
    DDSCreateTopologyFailed,
    DDSCommanderInfoFailed,
    DDSSubmitAgentsFailed,
    DDSActivateTopologyFailed,
    DDSSubscribeToSessionFailed,

    FairMQCreateTopologyFailed = 300,
    FairMQChangeStateFailed,
    FairMQGetStateFailed,
    FairMQSetPropertiesFailed,
    FairMQWaitForStateFailed,

    RuntimeError = 400
};

struct ErrorCategory : std::error_category
{
    const char* name() const noexcept override { return "odc"; }
    std::string message(int condition) const override
    {
        switch (static_cast<ErrorCode>(condition)) {
            // clang-format off
            case ErrorCode::RequestNotSupported:                return "Request not supported";
            case ErrorCode::RequestTimeout:                     return "Request timeout";
            case ErrorCode::ResourcePluginFailed:               return "Resource plugin failed";
            case ErrorCode::OperationInProgress:                return "async operation already in progress";
            case ErrorCode::OperationTimeout:                   return "async operation timed out";
            case ErrorCode::OperationCanceled:                  return "async operation canceled";
            case ErrorCode::DeviceChangeStateFailed:            return "Failed to change state of a FairMQ device";
            case ErrorCode::DeviceChangeStateInvalidTransition: return "Requested transition is not valid from the current state";
            case ErrorCode::DeviceGetPropertiesFailed:          return "Failed to get FairMQ device properties";
            case ErrorCode::DeviceSetPropertiesFailed:          return "Failed to set FairMQ device properties";
            case ErrorCode::TopologyFailed:                     return "Failed topology";

            case ErrorCode::DDSCreateSessionFailed:             return "Failed to create a DDS session";
            case ErrorCode::DDSShutdownSessionFailed:           return "Failed to shutdown a DDS session";
            case ErrorCode::DDSAttachToSessionFailed:           return "Failed to attach to a DDS session";
            case ErrorCode::DDSCreateTopologyFailed:            return "Failed to create DDS topology";
            case ErrorCode::DDSCommanderInfoFailed:             return "Failed to receive DDS commander info";
            case ErrorCode::DDSSubmitAgentsFailed:              return "Failed to submit DDS agents";
            case ErrorCode::DDSActivateTopologyFailed:          return "Failed to activate DDS topology";

            case ErrorCode::FairMQCreateTopologyFailed:         return "Failed to create FairMQ topology";
            case ErrorCode::FairMQChangeStateFailed:            return "Failed to change FairMQ device state";
            case ErrorCode::FairMQGetStateFailed:               return "Failed to get FairMQ device state";
            case ErrorCode::FairMQSetPropertiesFailed:          return "Failed to set FairMQ device properties";
            case ErrorCode::FairMQWaitForStateFailed:           return "Failed waiting for FairMQ device state";

            case ErrorCode::RuntimeError:                       return "Runtime error";
            default:                                            return "Unknown error";
            // clang-format on
        }
    }
};

static ErrorCategory errorCategory;

inline std::error_code MakeErrorCode(ErrorCode ec) { return { static_cast<int>(ec), errorCategory }; }

} // namespace odc::core

namespace std
{
template<>
struct is_error_code_enum<odc::core::ErrorCode> : true_type
{
};
} // namespace std

#endif /* defined(__ODC__Error__) */
