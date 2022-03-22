/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
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
    std::string message(int condition) const override;
};

std::error_code MakeErrorCode(ErrorCode);
} // namespace odc::core

namespace std
{
template<>
struct is_error_code_enum<odc::core::ErrorCode> : true_type
{
};
} // namespace std

#endif /* defined(__ODC__Error__) */
