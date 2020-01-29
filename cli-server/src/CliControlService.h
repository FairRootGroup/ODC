// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliControlService__
#define __ODC__CliControlService__

// ODC
#include "ControlService.h"

namespace odc
{
    namespace cli
    {
        class CCliControlService
        {

          public:
            CCliControlService();

          public:
            void Initialize(const std::string& _rmsPlugin,
                            const std::string& _configFile,
                            const std::string& _topologyFile);
            void ConfigureRun();
            void Start();
            void Stop();
            void Terminate();
            void Shutdown();
            void UpdateTopology(const std::string& _topologyFile);

          private:
            void printGeneralReply(const odc::core::SReturnValue& _value);

          private:
            std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
        };
    } // namespace cli
} // namespace odc

#endif /* defined(__ODC__CliControlService__) */
