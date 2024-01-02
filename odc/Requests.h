/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_REQUESTS
#define ODC_CORE_REQUESTS

#include <odc/Error.h>
#include <odc/Timer.h>
#include <odc/Params.h>
#include <odc/TopologyDefs.h>

#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace odc::core
{

template<typename T>
struct Request
{
    Request() = default;

    RequestResult mResult; ///< Request result
    size_t mTimeout = 0;   ///< Request timeout in seconds. 0 means "not set"

    Timer mTimer;          ///< Measure request processing time

    static std::string_view nameImpl() { return static_cast<T*>(this)->nameImpl(); }

    friend std::ostream& operator<<(std::ostream& os, const Request& r)
    {
        return os << " PartitionID: "     << std::quoted(r.mResult.mPartitionID)
                  << "; RunNr: "          << r.mResult.mRunNr
                  << "; DDS Session ID: " << r.mResult.mDDSSessionID
                  << "; Timeout: "        << r.mTimeout;
    }
};

struct InitializeRequest : public Request<InitializeRequest>
{
    InitializeRequest() {}
    InitializeRequest(const std::string& sessionID)
        : mTargetDDSSessionID(sessionID)
    {}

    std::string mTargetDDSSessionID; ///< DDS session ID

    static std::string_view nameImpl() { return "Initialize"; }

    friend std::ostream& operator<<(std::ostream& os, const InitializeRequest& ir)
    {
        return os << nameImpl() << " Request: "
                  << static_cast<const Request<InitializeRequest>&>(ir)
                  << "; Target DDS Session ID: " << quoted(ir.mTargetDDSSessionID);
    }
};

} // namespace odc::core

#endif /* defined(ODC_CORE_REQUESTS) */