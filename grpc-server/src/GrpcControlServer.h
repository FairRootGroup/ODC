// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__GrpcControlServer__
#define __ODC__GrpcControlServer__

// STD
#include <string>
// ODC
#include "GrpcControlService.h"

namespace odc
{
    namespace grpc
    {
        class CGrpcControlServer final
        {
          public:
            void Run(const std::string& _host);

            void setSubmitParams(const odc::core::SSubmitParams& _params);
            void setRecoTopoPath(const std::string& _path);
            void setQCTopoPath(const std::string& _path);

          private:
            odc::core::SSubmitParams m_submitParams; ///< Parameters of the submit request
            std::string m_recoTopoPath;              ///< Topology path of reco devices
            std::string m_qcTopoPath;                ///< Topology path of QC devices
        };
    } // namespace grpc
} // namespace odc

#endif /* defined(__ODC__GrpcControlServer__) */
