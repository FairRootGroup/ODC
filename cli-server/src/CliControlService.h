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

            std::string requestInitialize(const odc::core::SInitializeParams& _params);
            std::string requestSubmit(const odc::core::SSubmitParams& _params);
            std::string requestActivate(const odc::core::SActivateParams& _params);
            std::string requestUpscale(const odc::core::SUpdateParams& _params);
            std::string requestDownscale(const odc::core::SUpdateParams& _params);
            std::string requestConfigure();
            std::string requestStart();
            std::string requestStop();
            std::string requestReset();
            std::string requestTerminate();
            std::string requestShutdown();

          private:
            std::string generalReply(const odc::core::SReturnValue& _value);

          private:
            std::shared_ptr<odc::core::CControlService> m_service; ///< Core ODC service
        };
    } // namespace cli
} // namespace odc

#endif /* defined(__ODC__CliControlService__) */
