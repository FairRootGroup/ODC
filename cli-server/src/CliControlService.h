// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliControlService__
#define __ODC__CliControlService__

// ODC
#include "CliServiceHelper.h"
#include "ControlService.h"

namespace odc
{
    namespace cli
    {
        class CCliControlService : public odc::core::CCliServiceHelper<CCliControlService>
        {
            using hlp = odc::core::CCliServiceHelper<CCliControlService>;

          public:
            CCliControlService();

            void setRecoTopoPath(const std::string& _path);
            void setQCTopoPath(const std::string& _path);

            std::string requestInitialize(const odc::core::SInitializeParams& _params);
            std::string requestSubmit(const odc::core::SSubmitParams& _params);
            std::string requestActivate(const odc::core::SActivateParams& _params);
            std::string requestUpscale(const odc::core::SUpdateParams& _params);
            std::string requestDownscale(const odc::core::SUpdateParams& _params);
            std::string requestConfigure(hlp::EDeviceType _deviceType);
            std::string requestStart(hlp::EDeviceType _deviceType);
            std::string requestStop(hlp::EDeviceType _deviceType);
            std::string requestReset(hlp::EDeviceType _deviceType);
            std::string requestTerminate(hlp::EDeviceType _deviceType);
            std::string requestShutdown();

          private:
            std::string generalReply(const odc::core::SReturnValue& _value);
            std::string pathForDevice(hlp::EDeviceType _type);

          private:
            std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
            std::string m_recoTopoPath;                            ///< Topology path of reco devices
            std::string m_qcTopoPath;                              ///< Topology path of QC devices
        };
    } // namespace cli
} // namespace odc

#endif /* defined(__ODC__CliControlService__) */
