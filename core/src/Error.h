// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__Error__
#define __ODC__Error__

// STD
#include <system_error>

namespace odc::core
{
    enum class ErrorCode
    {
        RequestNotSupported = 100,
        RequestTimeout,
        ResourcePluginFailed,

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
