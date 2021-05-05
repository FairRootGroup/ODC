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
// READLINE
#ifdef READLINE_AVAIL
#include <readline/history.h>
#include <readline/readline.h>
#endif

namespace bpo = boost::program_options;

namespace odc::core
{
    template <typename OwnerT>
    class CCliServiceHelper
    {
      public:
        /// \brief Run the service
        /// \param[in] _cmds Array of requests. If empty than command line input is required.
        void run(const std::vector<std::string>& _cmds = std::vector<std::string>())
        {
            printDescription();

            // Read the input from commnad line
            if (_cmds.empty())
            {
#ifdef READLINE_AVAIL
                // Register command completion handler
                rl_attempted_completion_function = &CCliServiceHelper::commandCompleter;
#endif
                while (true)
                {
                    std::string cmd;
#ifdef READLINE_AVAIL
                    char* buf{ readline(">> ") };
                    if (buf != nullptr)
                    {
                        cmd = std::string(buf);
                        free(buf);
                    }
                    else
                    {
                        OLOG(ESeverity::clean) << std::endl;
                        break; // ^D
                    }

                    if (!cmd.empty())
                    {
                        add_history(cmd.c_str());
                    }
#else
                    OLOG(ESeverity::clean) << "Please enter command: ";
                    getline(std::cin, cmd);
#endif

                    boost::trim_right(cmd);

                    processRequest(cmd);
                }
            }
            else
            {
                // Execute consequently all commands
                execCmds(_cmds);
                // Exit at the end
                exit(EXIT_SUCCESS);
            }
        }

      private:
#ifdef READLINE_AVAIL
        static char* commandGenerator(const char* text, int index)
        {
            static const std::vector<std::string> commands{ ".quit",  ".init",    ".submit",    ".activate", ".run",
                                                            ".prop",  ".upscale", ".downscale", ".state",    ".config",
                                                            ".start", ".stop",    ".reset",     ".term",     ".down",
                                                            ".batch", ".sleep" };
            static std::vector<std::string> matches;

            if (index == 0)
            {
                matches.clear();
                for (const auto& cmd : commands)
                {
                    if (boost::starts_with(cmd, text))
                    {
                        matches.push_back(cmd);
                    }
                }
            }

            if (index < int(matches.size()))
            {
                return strdup(matches[index].c_str());
            }
            return nullptr;
        }

        static char** commandCompleter(const char* text, int start, int /*end*/)
        {
            // Uncomment to disable filename completion
            // rl_attempted_completion_over = 1;

            // Use command completion only for the first position
            if (start == 0)
            {
                return rl_completion_matches(text, &CCliServiceHelper::commandGenerator);
            }
            // Returning nullptr here will make readline use the default filename completer
            return nullptr;
        }
#endif

        void execCmds(const std::vector<std::string>& _cmds)
        {
            for (const auto& cmd : _cmds)
            {
                OLOG(ESeverity::clean) << "Executing command " << std::quoted(cmd);
                processRequest(cmd);
            }
        }

        void execBatch(const std::vector<std::string>& _args)
        {
            CCliHelper::SBatchOptions bopt;
            if (parseCommand(_args, bopt))
            {
                execCmds(bopt.m_outputCmds);
            }
        }

        void execSleep(const std::vector<std::string>& _args)
        {
            CCliHelper::SSleepOptions sopt;
            if (parseCommand(_args, sopt))
            {
                if (sopt.m_ms > 0)
                {
                    OLOG(ESeverity::clean) << "Sleeping " << sopt.m_ms << " ms";
                    std::this_thread::sleep_for(std::chrono::milliseconds(sopt.m_ms));
                }
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
                        std::apply([this](auto&&... args) { ((this->print(args)), ...); }, std::tie(params...));
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
            else if (cmd == ".sleep")
            {
                execSleep(args);
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
                                   << ".batch - Execute an array of requests." << std::endl
                                   << ".sleep - Sleep for X ms." << std::endl;
        }
    };
} // namespace odc::core

#endif /* defined(__ODC__CliServiceHelper__) */
