// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__Error__
#define __ODC__Error__

// ODC
#include "MiscUtils.h"
// STD
#include <stdexcept>
#include <system_error>

namespace odc::core
{
    struct RuntimeError : ::std::runtime_error
    {
        template <typename... T>
        explicit RuntimeError(T&&... t)
            : ::std::runtime_error::runtime_error(toString(std::forward<T>(t)...))
        {
        }
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
        DeviceGetPropertiesFailed,
        DeviceSetPropertiesFailed,
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
        FairMQSetPropertiesFailed
    };

    struct ErrorCategory : std::error_category
    {
        const char* name() const noexcept override;
        std::string message(int _condition) const override;
    };

    std::error_code MakeErrorCode(ErrorCode);
} // namespace odc::core

namespace std
{
    template <>
    struct is_error_code_enum<odc::core::ErrorCode> : true_type
    {
    };
} // namespace std

#endif /* defined(__ODC__Error__) */
