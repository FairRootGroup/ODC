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
// BOOST
#include <boost/algorithm/string.hpp>

namespace odc
{
    namespace core
    {
        template <typename OwnerT>
        class CCliServiceHelper
        {
          public:
            enum class EDeviceType
            {
                all,
                reco,
                qc
            };

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
            EDeviceType stringToDeviceType(const std::string& _str)
            {
                if (_str == "reco")
                    return EDeviceType::reco;
                else if (_str == "qc")
                    return EDeviceType::qc;
                return EDeviceType::all;
            }

            void processRequest(const std::string& _cmd)
            {
                OwnerT* p = reinterpret_cast<OwnerT*>(this);

                std::string replyString;

                std::vector<std::string> cmds;
                boost::split(cmds, _cmd, boost::is_any_of("\t "));
                std::string cmd{ cmds.empty() ? "" : cmds.front() };
                std::string par{ cmds.size() > 1 ? cmds[1] : "" };

                if (cmd.empty())
                {
                }
                else if (cmd == ".quit")
                {
                    exit(EXIT_SUCCESS);
                }
                else if (cmd == ".init")
                {
                    OLOG(ESeverity::clean) << "Sending initialization request...";
                    replyString = p->requestInitialize(m_initializeParams);
                }
                else if (cmd == ".submit")
                {
                    OLOG(ESeverity::clean) << "Sending submit request...";
                    replyString = p->requestSubmit(m_submitParams);
                }
                else if (cmd == ".activate")
                {
                    OLOG(ESeverity::clean) << "Sending activate request...";
                    replyString = p->requestActivate(m_activateParams);
                }
                else if (cmd == ".upscale")
                {
                    OLOG(ESeverity::clean) << "Sending upscale request...";
                    replyString = p->requestUpscale(m_upscaleParams);
                }
                else if (cmd == ".downscale")
                {
                    OLOG(ESeverity::clean) << "Sending downscale request...";
                    replyString = p->requestDownscale(m_downscaleParams);
                }
                else if (cmd == ".config")
                {
                    OLOG(ESeverity::clean) << "Sending configure run request...";
                    replyString = p->requestConfigure(stringToDeviceType(par));
                }
                else if (cmd == ".start")
                {
                    OLOG(ESeverity::clean) << "Sending start request...";
                    replyString = p->requestStart(stringToDeviceType(par));
                }
                else if (cmd == ".stop")
                {
                    OLOG(ESeverity::clean) << "Sending stop request...";
                    replyString = p->requestStop(stringToDeviceType(par));
                }
                else if (cmd == ".reset")
                {
                    OLOG(ESeverity::clean) << "Sending reset request...";
                    replyString = p->requestReset(stringToDeviceType(par));
                }
                else if (cmd == ".term")
                {
                    OLOG(ESeverity::clean) << "Sending terminate request...";
                    replyString = p->requestTerminate(stringToDeviceType(par));
                }
                else if (cmd == ".down")
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
                                       << ".config (all|reco|qc) - Configure run request." << std::endl
                                       << ".start (all|reco|qc) - Start request." << std::endl
                                       << ".stop (all|reco|qc) - Stop request." << std::endl
                                       << ".reset (all|reco|qc) - Reset request." << std::endl
                                       << ".term (all|reco|qc) - Terminate request." << std::endl
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
