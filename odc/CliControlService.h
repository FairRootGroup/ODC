/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__CliControlService__
#define __ODC__CliControlService__

// STD
#include <chrono>
// ODC
#include <odc/CliServiceHelper.h>
#include <odc/Controller.h>
#include <odc/DDSSubmit.h>

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

        std::string requestInitialize(const odc::core::CommonParams& _common,
                                      const odc::core::SInitializeParams& _params);
        std::string requestSubmit(const odc::core::CommonParams& _common, const odc::core::SSubmitParams& _params);
        std::string requestActivate(const odc::core::CommonParams& _common, const odc::core::SActivateParams& _params);
        std::string requestRun(const odc::core::CommonParams& _common,
                               const odc::core::SInitializeParams& _initializeParams,
                               const odc::core::SSubmitParams& _submitParams,
                               const odc::core::SActivateParams& _activateParams);
        std::string requestUpscale(const odc::core::CommonParams& _common, const odc::core::SUpdateParams& _params);
        std::string requestDownscale(const odc::core::CommonParams& _common, const odc::core::SUpdateParams& _params);
        std::string requestGetState(const odc::core::CommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestSetProperties(const odc::core::CommonParams& _common,
                                         const odc::core::SetPropertiesParams& _params);
        std::string requestConfigure(const odc::core::CommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestStart(const odc::core::CommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestStop(const odc::core::CommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestReset(const odc::core::CommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestTerminate(const odc::core::CommonParams& _common, const odc::core::SDeviceParams& _params);
        std::string requestShutdown(const odc::core::CommonParams& _common);
        std::string requestStatus(const odc::core::SStatusParams& _params);

      private:
        std::string generalReply(const odc::core::RequestResult& result);
        std::string statusReply(const odc::core::StatusRequestResult& result);

      private:
        std::shared_ptr<odc::core::Controller> m_service; ///< Core ODC service
    };
} // namespace odc::cli

#endif /* defined(__ODC__CliControlService__) */
