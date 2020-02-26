// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliServiceHelper__
#define __ODC__CliServiceHelper__

// ODC
#include "ControlService.h"
#include "Logger.h"
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
            void run()
            {
                printDescription();

                while (true)
                {
                    std::string cmd;
                    OLOG(ESeverity::clean) << "Please enter command: ";
                    getline(std::cin, cmd);
                    processRequest(cmd);
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
                    OLOG(ESeverity::clean) << "Sending initialization request...";
                    replyString = p->requestInitialize(m_initializeParams);
                }
                else if (_cmd == ".submit")
                {
                    OLOG(ESeverity::clean) << "Sending submit request...";
                    replyString = p->requestSubmit(m_submitParams);
                }
                else if (_cmd == ".activate")
                {
                    OLOG(ESeverity::clean) << "Sending activate request...";
                    replyString = p->requestActivate(m_activateParams);
                }
                else if (_cmd == ".upscale")
                {
                    OLOG(ESeverity::clean) << "Sending upscale request...";
                    replyString = p->requestUpscale(m_upscaleParams);
                }
                else if (_cmd == ".downscale")
                {
                    OLOG(ESeverity::clean) << "Sending downscale request...";
                    replyString = p->requestDownscale(m_downscaleParams);
                }
                else if (_cmd == ".config")
                {
                    OLOG(ESeverity::clean) << "Sending configure run request...";
                    replyString = p->requestConfigure();
                }
                else if (_cmd == ".start")
                {
                    OLOG(ESeverity::clean) << "Sending start request...";
                    replyString = p->requestStart();
                }
                else if (_cmd == ".stop")
                {
                    OLOG(ESeverity::clean) << "Sending stop request...";
                    replyString = p->requestStop();
                }
                else if (_cmd == ".reset")
                {
                    OLOG(ESeverity::clean) << "Sending reset request...";
                    replyString = p->requestReset();
                }
                else if (_cmd == ".term")
                {
                    OLOG(ESeverity::clean) << "Sending terminate request...";
                    replyString = p->requestTerminate();
                }
                else if (_cmd == ".down")
                {
                    OLOG(ESeverity::clean) << "Sending shutdown request...";
                    replyString = p->requestShutdown();
                }
                else
                {
                    OLOG(ESeverity::clean) << "Unknown command " << _cmd;
                }

                if (!replyString.empty())
                {
                    OLOG(ESeverity::clean) << "Reply: " << replyString;
                }
            }

            void printDescription()
            {
                OLOG(ESeverity::clean) << "Sample client for ODC service." << std::endl
                                       << "Available commands:" << std::endl
                                       << ".quit - Quit the program." << std::endl
                                       << ".init - Initialization request." << std::endl
                                       << ".submit - Submit request." << std::endl
                                       << ".activate - Activate request." << std::endl
                                       << ".upscale - Upscale topology request." << std::endl
                                       << ".downscale - Downscale topology request." << std::endl
                                       << ".config - Configure run request." << std::endl
                                       << ".start - Start request." << std::endl
                                       << ".stop - Stop request." << std::endl
                                       << ".term - Terminate request." << std::endl
                                       << ".down - Shutdown request." << std::endl;
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
