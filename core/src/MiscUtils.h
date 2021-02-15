// Copyright 2014 GSI, Inc. All rights reserved.
//
// This file contains a number of helpers to calculate execution time of a function.
//
#ifndef __ODC__MiscUtils__
#define __ODC__MiscUtils__

// STD
#include <string>
// SYS
#include <stdlib.h>
// ODC
#include "Logger.h"

namespace odc::core
{
    void setupGrpcVerbosity(const CLogger::SConfig& _config)
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
} // namespace odc::core
#endif /*__ODC__MiscUtils__*/
