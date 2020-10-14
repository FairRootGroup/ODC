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
          public:
            CCliControlService();

            std::string requestInitialize(odc::core::partitionID_t _partitionID,
                                          const odc::core::SInitializeParams& _params);
            std::string requestSubmit(odc::core::partitionID_t _partitionID, const odc::core::SSubmitParams& _params);
            std::string requestActivate(odc::core::partitionID_t _partitionID,
                                        const odc::core::SActivateParams& _params);
            std::string requestRun(odc::core::partitionID_t _partitionID,
                                   const odc::core::SInitializeParams& _initializeParams,
                                   const odc::core::SSubmitParams& _submitParams,
                                   const odc::core::SActivateParams& _activateParams);
            std::string requestUpscale(odc::core::partitionID_t _partitionID, const odc::core::SUpdateParams& _params);
            std::string requestDownscale(odc::core::partitionID_t _partitionID,
                                         const odc::core::SUpdateParams& _params);
            std::string requestGetState(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
            std::string requestSetProperties(odc::core::partitionID_t _partitionID,
                                             const odc::core::SSetPropertiesParams& _params);
            std::string requestConfigure(odc::core::partitionID_t _partitionID,
                                         const odc::core::SDeviceParams& _params);
            std::string requestStart(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
            std::string requestStop(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
            std::string requestReset(odc::core::partitionID_t _partitionID, const odc::core::SDeviceParams& _params);
            std::string requestTerminate(odc::core::partitionID_t _partitionID,
                                         const odc::core::SDeviceParams& _params);
            std::string requestShutdown(odc::core::partitionID_t _partitionID);

          private:
            std::string generalReply(const odc::core::SReturnValue& _value);

          private:
            std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
        };
    } // namespace cli
} // namespace odc

#endif /* defined(__ODC__CliControlService__) */
