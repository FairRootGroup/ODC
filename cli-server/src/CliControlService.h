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
            CCliControlService(const std::string& _rmsPlugin,
                               const std::string& _configFile,
                               const std::string& _topologyFile);

          public:
            void Initialize();
            void ConfigureRun();
            void Start();
            void Stop();
            void Terminate();
            void Shutdown();

          private:
            void printGeneralReply(const odc::core::SReturnValue& _value);

          private:
            std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
            std::string m_rmsPlugin;                               ///< RMS plugin used for DDS
            std::string m_configFile;                              ///< Configuration file for RMS plugin
            std::string m_topologyFile;                            ///< Topology file
        };
    } // namespace cli
} // namespace odc

#endif /* defined(__ODC__CliControlService__) */
