// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliServiceHelper__
#define __ODC__CliServiceHelper__

// ODC
#include "ControlService.h"
// STD
#include <iostream>

namespace odc
{
    namespace core
    {
        template <typename OwnerT>
        class CCliServiceHelper
        {
          public:
            void processRequest(const std::string& _cmd)
            {
                OwnerT* p = reinterpret_cast<OwnerT*>(this);

                std::string replyString;

                if (_cmd.empty())
                {
                }
                else if (_cmd == ".quit")
                {
                    exit(EXIT_SUCCESS);
                }
                else if (_cmd == ".init")
                {
                    std::cout << "Sending initialization request..." << std::endl;
                    replyString = p->requestInitialize(m_initializeParams);
                }
                else if (_cmd == ".submit")
                {
                    std::cout << "Sending submit request..." << std::endl;
                    replyString = p->requestSubmit(m_submitParams);
                }
                else if (_cmd == ".activate")
                {
                    std::cout << "Sending activate request..." << std::endl;
                    replyString = p->requestActivate(m_activateParams);
                }
                else if (_cmd == ".upscale")
                {
                    std::cout << "Sending upscale request..." << std::endl;
                    replyString = p->requestUpscale(m_upscaleParams);
                }
                else if (_cmd == ".downscale")
                {
                    std::cout << "Sending downscale request..." << std::endl;
                    replyString = p->requestDownscale(m_downscaleParams);
                }
                else if (_cmd == ".config")
                {
                    std::cout << "Sending configure run request..." << std::endl;
                    replyString = p->requestConfigure();
                }
                else if (_cmd == ".start")
                {
                    std::cout << "Sending start request..." << std::endl;
                    replyString = p->requestStart();
                }
                else if (_cmd == ".stop")
                {
                    std::cout << "Sending stop request..." << std::endl;
                    replyString = p->requestStop();
                }
                else if (_cmd == ".reset")
                {
                    std::cout << "Sending reset request..." << std::endl;
                    replyString = p->requestReset();
                }
                else if (_cmd == ".term")
                {
                    std::cout << "Sending terminate request..." << std::endl;
                    replyString = p->requestTerminate();
                }
                else if (_cmd == ".down")
                {
                    std::cout << "Sending shutdown request..." << std::endl;
                    replyString = p->requestShutdown();
                }
                else
                {
                    std::cout << "Unknown command " << _cmd << std::endl;
                }

                if (!replyString.empty())
                {
                    std::cout << "Reply: " << replyString << std::endl;
                }
            }

            void setInitializeParams(const odc::core::SInitializeParams& _initializeParams)
            {
                m_initializeParams = _initializeParams;
            }

            void setSubmitParams(const odc::core::SSubmitParams& _submitParams)
            {
                m_submitParams = _submitParams;
            }
            void setActivateParams(const odc::core::SActivateParams& _activateParams)
            {
                m_activateParams = _activateParams;
            }
            void setUpscaleParams(const odc::core::SUpdateParams& _upscaleParams)
            {
                m_upscaleParams = _upscaleParams;
            }
            void setDownscaleParams(const odc::core::SUpdateParams& _downscaleParams)
            {
                m_downscaleParams = _downscaleParams;
            }

          private:
            odc::core::SInitializeParams m_initializeParams;
            odc::core::SSubmitParams m_submitParams;
            odc::core::SActivateParams m_activateParams;
            odc::core::SUpdateParams m_upscaleParams;
            odc::core::SUpdateParams m_downscaleParams;
        };
    } // namespace core
} // namespace odc

#endif /* defined(__ODC__CliServiceHelper__) */
