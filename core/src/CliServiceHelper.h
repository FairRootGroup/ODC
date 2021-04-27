// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__CliServiceHelper__
#define __ODC__CliServiceHelper__

// ODC
#include "CliHelper.h"
#include "ControlService.h"
#include "Logger.h"
// STD
#include <iostream>
#include <tuple>
// BOOST
#include <boost/algorithm/string.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

namespace bpo = boost::program_options;

namespace odc::core
{
    template <typename OwnerT>
    class CCliServiceHelper
    {
      public:
        /// \brief Run the service
        /// \param[in] _cmds Array of requests. If empty than command line input is required.
        /// \param[in] _delay Delay between command execution.
        void run(const std::vector<std::string>& _cmds = std::vector<std::string>(),
                 const std::chrono::milliseconds& _delay = std::chrono::milliseconds(1000))
        {
            printDescription();

            // Read the input from commnad line
            if (_cmds.empty())
            {
                while (true)
                {
                    std::string cmd;
                    OLOG(ESeverity::clean) << "Please enter command: ";
                    getline(std::cin, cmd);
                    processRequest(cmd);
                }
            }
            else
            {
                // Execute consequently all commands
                execCmds(_cmds, _delay);
                // Exit at the end
                exit(EXIT_SUCCESS);
            }
        }

      private:
        void execCmds(const std::vector<std::string>& _cmds, const std::chrono::milliseconds& _delay)
        {
            for (const auto& cmd : _cmds)
            {
                OLOG(ESeverity::clean) << "Executing command " << std::quoted(cmd);
                processRequest(cmd);
                OLOG(ESeverity::clean) << "Waiting " << _delay.count() << " ms";
                if (_delay.count() > 0)
                {
                    std::this_thread::sleep_for(_delay);
                }
            }
        }

        void execBatch(const std::vector<std::string>& _args)
        {
            CCliHelper::SBatchOptions bopt;
            if (parseCommand(_args, bopt))
            {
                execCmds(bopt.m_outputCmds, std::chrono::milliseconds(1000));
            }
        }

        template <typename... RequestParams_t>
        bool parseCommand(const std::vector<std::string>& _args, RequestParams_t&&... _params)
        {
            try
            {
                // Options description: generic + request specific
                bpo::options_description options("Request options");
                CCliHelper::addHelpOptions(options);

                // Loop over input parameters and add program options
                std::apply([&options](auto&&... args) { ((CCliHelper::addOptions(options, args)), ...); },
                           std::tie(_params...));

                // Parsing command-line
                bpo::variables_map vm;
                bpo::store(bpo::command_line_parser(_args).options(options).run(), vm);
                bpo::notify(vm);

                CCliHelper::parseOptions(vm, _params...);

                if (vm.count("help"))
                {
                    OLOG(ESeverity::clean) << options;
                    return false;
                }
            }
            catch (std::exception& _e)
            {
                OLOG(ESeverity::clean) << "Error parsing options: " << _e.what();
                return false;
            }
            return true;
        }

        template <typename T>
        void print(const T& _value)
        {
            OLOG(ESeverity::clean) << _value;
        }

        template <typename... RequestParams_t, typename StubFunc_t>
        std::string request(const std::string& _msg, const std::vector<std::string>& _args, StubFunc_t _stubFunc)
        {
            std::string result;
            std::tuple<RequestParams_t...> tuple;
            std::apply(
                [&result, &_msg, &_args, &_stubFunc, this](auto&&... params) {
                    partitionID_t partitionID;
                    if (parseCommand(_args, partitionID, params...))
                    {
                        OLOG(ESeverity::clean) << "Partition <" << partitionID << ">: " << _msg;
                        std::apply([this](auto&&... args) { ((print(args)), ...); }, std::tie(params...));
                        OwnerT* p = reinterpret_cast<OwnerT*>(this);
                        result = (p->*_stubFunc)(partitionID, params...);
                    }
                },
                tuple);
            return result;
        }

        void processRequest(const std::string& _cmd)
        {
            if (_cmd == ".quit")
            {
                exit(EXIT_SUCCESS);
            }

            std::string replyString;
            std::vector<std::string> args{ bpo::split_unix(_cmd) };
            std::string cmd{ args.empty() ? "" : args.front() };

            if (cmd == ".init")
            {
                replyString =
                    request<SInitializeParams>("sending Initialize request...", args, &OwnerT::requestInitialize);
            }
            else if (cmd == ".submit")
            {
                replyString = request<SSubmitParams>("sending Submit request...", args, &OwnerT::requestSubmit);
            }
            else if (cmd == ".activate")
            {
                replyString = request<SActivateParams>("sending Activate request...", args, &OwnerT::requestActivate);
            }
            else if (cmd == ".run")
            {
                replyString = request<SInitializeParams, SSubmitParams, SActivateParams>(
                    "sending Run request...", args, &OwnerT::requestRun);
            }
            else if (cmd == ".upscale")
            {
                replyString = request<SUpdateParams>("sending Upscale request...", args, &OwnerT::requestUpscale);
            }
            else if (cmd == ".downscale")
            {
                replyString = request<SUpdateParams>("sending Downscale request...", args, &OwnerT::requestDownscale);
            }
            else if (cmd == ".config")
            {
                replyString = request<SDeviceParams>("sending Configure request...", args, &OwnerT::requestConfigure);
            }
            else if (cmd == ".state")
            {
                replyString = request<SDeviceParams>("sending GetState request...", args, &OwnerT::requestGetState);
            }
            else if (cmd == ".prop")
            {
                replyString = request<SSetPropertiesParams>(
                    "sending SetProperties request...", args, &OwnerT::requestSetProperties);
            }
            else if (cmd == ".start")
            {
                replyString = request<SDeviceParams>("sending Start request...", args, &OwnerT::requestStart);
            }
            else if (cmd == ".stop")
            {
                replyString = request<SDeviceParams>("sending Stop request...", args, &OwnerT::requestStop);
            }
            else if (cmd == ".reset")
            {
                replyString = request<SDeviceParams>("sending Reset request...", args, &OwnerT::requestReset);
            }
            else if (cmd == ".term")
            {
                replyString = request<SDeviceParams>("sending Terminate request...", args, &OwnerT::requestTerminate);
            }
            else if (cmd == ".down")
            {
                replyString = request<>("sending Shutdown request...", args, &OwnerT::requestShutdown);
            }
            else if (cmd == ".batch")
            {
                execBatch(args);
            }
            else
            {
                OLOG(ESeverity::clean) << "Unknown command " << _cmd;
            }

            if (!replyString.empty())
            {
                OLOG(ESeverity::clean) << "Reply: (\n" << replyString << ")";
            }
        }

        void printDescription()
        {
            OLOG(ESeverity::clean) << "Sample client for ODC service." << std::endl
                                   << "Each command has a set of extra options. Use " << std::quoted("--help")
                                   << " to list available options." << std::endl
                                   << "For example, " << std::quoted(".activate --topo topo_file.xml")
                                   << " command activates a topology " << std::quoted("topo_file.xml") << "."
                                   << std::endl
                                   << "List of available commands:" << std::endl
                                   << ".quit - Quit the program." << std::endl
                                   << ".init - Initialization request." << std::endl
                                   << ".submit - Submit request." << std::endl
                                   << ".activate - Activate request." << std::endl
                                   << ".run - Run request." << std::endl
                                   << ".prop - Set properties request." << std::endl
                                   << ".upscale - Upscale topology request." << std::endl
                                   << ".downscale - Downscale topology request." << std::endl
                                   << ".state - Get state request." << std::endl
                                   << ".config - Configure run request." << std::endl
                                   << ".start - Start request." << std::endl
                                   << ".stop - Stop request." << std::endl
                                   << ".reset - Reset request." << std::endl
                                   << ".term - Terminate request." << std::endl
                                   << ".down - Shutdown request." << std::endl
                                   << ".batch - Execute an array of requests." << std::endl;
        }
    };
} // namespace odc::core

#endif /* defined(__ODC__CliServiceHelper__) */
