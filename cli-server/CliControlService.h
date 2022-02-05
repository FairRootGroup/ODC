// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliControlService__
#define __ODC__CliControlService__

// STD
#include <chrono>
// ODC
#include "CliServiceHelper.h"
#include "ControlService.h"
#include "DDSSubmit.h"

namespace odc::cli
{
    class CCliControlService : public odc::core::CCliServiceHelper<CCliControlService>
    {
      public:
        CCliControlService();

        void setTimeout(const std::chrono::seconds& _timeout);

        void registerResourcePlugins(const odc::core::CPluginManager::PluginMap_t& _pluginMap);
        void registerRequestTriggers(const odc::core::CPluginManager::PluginMap_t& _triggerMap);
        void restore(const std::string& _restoreId);

        std::string requestInitialize(const odc::core::SCommonParams& _common,
                                      const odc::core::SInitializeParams& _params);
        std::string requestSubmit(const odc::core::SCommonParams& _common, const odc::core::SSubmitParams& _params);
        std::string requestActivate(const odc::core::SCommonParams& _common, const odc::core::SActivateParams& _params);
        std::string requestRun(const odc::core::SCommonParams& _common,
                               const odc::core::SInitializeParams& _initializeParams,
                               const odc::core::SSubmitParams& _submitParams,
                               const odc::core::SActivateParams& _activateParams);
        std::string requestUpscale(const odc::core::SCommonParams& _common, const odc::core::SUpdateParams& _params);
        std::string requestDownscale(const odc::core::SCommonParams& _common, const odc::core::SUpdateParams& _params);
        std::string requestGetState(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestSetProperties(const odc::core::SCommonParams& _common,
                                         const odc::core::SSetPropertiesParams& _params);
        std::string requestConfigure(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestStart(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestStop(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestReset(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestTerminate(const odc::core::SCommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestShutdown(const odc::core::SCommonParams& _common);
        std::string requestStatus(const odc::core::SStatusParams& _params);

      private:
        std::string generalReply(const odc::core::SReturnValue& _value);
        std::string statusReply(const odc::core::SStatusReturnValue& _value);

      private:
        std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
    };
} // namespace odc::cli

#endif /* defined(__ODC__CliControlService__) */
