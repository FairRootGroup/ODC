// Copyright 2014 GSI, Inc. All rights reserved.
//
// This file contains a number of helpers to calculate execution time of a function.
//
#ifndef __ODC__MiscUtils__
#define __ODC__MiscUtils__

// STD
#include <initializer_list>
#include <sstream>
#include <string>
// SYS
#include <stdlib.h>
// ODC
#include "Logger.h"
// Boost
#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace odc::core
{
    inline void setupGrpcVerbosity(const CLogger::SConfig& _config)
    {
        // From gRPC docs: https://grpc.github.io/grpc/cpp/md_doc_environment_variables.html
        // GRPC_VERBOSITY Default gRPC logging verbosity - one of:
        // DEBUG - log all gRPC messages
        // INFO - log INFO and ERROR message
        // ERROR - log only errors (default)
        // NONE - won't log any
        std::string grpc;
        switch (_config.m_severity)
        {
            case ESeverity::debug:
                grpc = "DEBUG";
                break;
            case ESeverity::info:
                grpc = "INFO";
                break;
            case ESeverity::error:
                grpc = "ERROR";
                break;
            default:
                break;
        }
        if (grpc.length() > 0 && ::setenv("GRPC_VERBOSITY", grpc.c_str(), 1) == 0)
        {
            OLOG(ESeverity::info) << "Set GRPC_VERBOSITY to " << grpc;
        }
    }

    /// @brief concatenates a variable number of args with the << operator via a stringstream
    /// @param t objects to be concatenated
    /// @return concatenated string
    template<typename ... T>
    auto toString(T&&... t) -> std::string
    {
        std::stringstream ss;
        (void)std::initializer_list<int>{(ss << t, 0)...};
        return ss.str();
    }

    // generates UUID string
    inline std::string uuid()
    {
        boost::uuids::random_generator gen;
        boost::uuids::uuid u = gen();
        return boost::uuids::to_string(u);
    }

    // generates UUID and returns its hash
    inline std::size_t uuidHash()
    {
        boost::uuids::random_generator gen;
        boost::hash<boost::uuids::uuid> uuid_hasher;
        boost::uuids::uuid u = gen();
        return uuid_hasher(u);
    }
} // namespace odc::core
#endif /*__ODC__MiscUtils__*/
