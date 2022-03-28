/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__CliServiceHelper__
#define __ODC__CliServiceHelper__

// ODC
#include <odc/CliHelper.h>
#include <odc/Controller.h>
#include <odc/Logger.h>
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

namespace odc::core {

template<typename Owner>
class CCliServiceHelper
{
  public:
    /// \brief Run the service
    /// \param[in] _cmds Array of requests. If empty than command line input is required.
    void run(const std::vector<std::string>& _cmds = std::vector<std::string>())
    {
        printDescription();

        // Read the input from commnad line
        if (_cmds.empty()) {
#ifdef READLINE_AVAIL
            // Register command completion handler
            rl_attempted_completion_function = &CCliServiceHelper::commandCompleter;
#endif
            while (true) {
                std::string cmd;
#ifdef READLINE_AVAIL
                char* buf{ readline(">> ") };
                if (buf != nullptr) {
                    cmd = std::string(buf);
                    free(buf);
                } else {
                    std::cout << std::endl << std::endl;
                    break; // ^D
                }

                if (!cmd.empty()) {
                    add_history(cmd.c_str());
                }
#else
                std::cout << "Please enter command: " << std::cout;
                getline(std::cin, cmd);
#endif

                boost::trim_right(cmd);

                processRequest(cmd);
            }
        } else {
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
        static const std::vector<std::string> commands{
            ".quit",   ".init",  ".submit", ".activate", ".run",  ".prop", ".upscale", ".downscale", ".state",
            ".config", ".start", ".stop",   ".reset",    ".term", ".down", ".status",  ".batch",     ".sleep", ".help"
        };
        static std::vector<std::string> matches;

        if (index == 0) {
            matches.clear();
            for (const auto& cmd : commands) {
                if (boost::starts_with(cmd, text)) {
                    matches.push_back(cmd);
                }
            }
        }

        if (index < int(matches.size())) {
            return strdup(matches[index].c_str());
        }
        return nullptr;
    }

    static char** commandCompleter(const char* text, int start, int /*end*/)
    {
        // Uncomment to disable filename completion
        // rl_attempted_completion_over = 1;

        // Use command completion only for the first position
        if (start == 0) {
            return rl_completion_matches(text, &CCliServiceHelper::commandGenerator);
        }
        // Returning nullptr here will make readline use the default filename completer
        return nullptr;
    }
#endif

    void execCmds(const std::vector<std::string>& _cmds)
    {
        for (const auto& cmd : _cmds) {
            std::cout << "Executing command " << std::quoted(cmd) << std::endl;
            processRequest(cmd);
        }
    }

    void execBatch(const std::vector<std::string>& _args)
    {
        CCliHelper::SBatchOptions bopt;
        if (parseCommand(_args, bopt)) {
            execCmds(bopt.m_outputCmds);
        }
    }

    void execSleep(const std::vector<std::string>& _args)
    {
        CCliHelper::SSleepOptions sopt;
        if (parseCommand(_args, sopt)) {
            if (sopt.m_ms > 0) {
                std::cout << "Sleeping " << sopt.m_ms << " ms" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(sopt.m_ms));
            }
        }
    }

    template<typename... RequestParams_t>
    bool parseCommand(const std::vector<std::string>& _args, RequestParams_t&&... _params)
    {
        try {
            // Options description: generic + request specific
            bpo::options_description options("Request options");
            CCliHelper::addHelpOptions(options);

            // Loop over input parameters and add program options
            std::apply([&options](auto&&... args) { ((CCliHelper::addOptions(options, args)), ...); }, std::tie(_params...));

            // Parsing command-line
            bpo::variables_map vm;
            bpo::store(bpo::command_line_parser(_args).options(options).run(), vm);
            bpo::notify(vm);

            CCliHelper::parseOptions(vm, _params...);

            if (vm.count("help")) {
                std::cout << options << std::endl;
                return false;
            }
        } catch (std::exception& _e) {
            std::cout << "Error parsing options: " << _e.what() << std::endl;
            return false;
        }
        return true;
    }

    template<typename T>
    void print(const T& _value)
    {
        std::cout << _value << std::endl;
    }

    template<typename... RequestParams_t, typename StubFunc_t>
    std::string request(const std::string& _msg, const std::vector<std::string>& _args, StubFunc_t _stubFunc)
    {
        std::string result;
        std::tuple<RequestParams_t...> tuple;
        std::apply(
            [&result, &_msg, &_args, &_stubFunc, this](auto&&... params) {
                if (parseCommand(_args, params...)) {
                    std::cout << _msg << std::endl;
                    std::apply([this](auto&&... args) { ((this->print(args)), ...); }, std::tie(params...));
                    Owner* p = reinterpret_cast<Owner*>(this);
                    result = (p->*_stubFunc)(params...);
                }
            },
            tuple);
        return result;
    }

    void processRequest(const std::string& _cmd)
    {
        if (_cmd == ".quit") {
            exit(EXIT_SUCCESS);
        }

        std::string replyString;
        std::vector<std::string> args{ bpo::split_unix(_cmd) };
        std::string cmd{ args.empty() ? "" : args.front() };

        if (cmd == ".init") {
            replyString = request<CommonParams, SInitializeParams>("Sending Initialize request...", args, &Owner::requestInitialize);
        } else if (cmd == ".submit") {
            replyString = request<CommonParams, SSubmitParams>("Sending Submit request...", args, &Owner::requestSubmit);
        } else if (cmd == ".activate") {
            replyString = request<CommonParams, SActivateParams>("Sending Activate request...", args, &Owner::requestActivate);
        } else if (cmd == ".run") {
            replyString = request<CommonParams, SInitializeParams, SSubmitParams, SActivateParams>("Sending Run request...", args, &Owner::requestRun);
        } else if (cmd == ".upscale") {
            replyString = request<CommonParams, SUpdateParams>("Sending Upscale request...", args, &Owner::requestUpscale);
        } else if (cmd == ".downscale") {
            replyString = request<CommonParams, SUpdateParams>("Sending Downscale request...", args, &Owner::requestDownscale);
        } else if (cmd == ".config") {
            replyString = request<CommonParams, SDeviceParams>("Sending Configure request...", args, &Owner::requestConfigure);
        } else if (cmd == ".state") {
            replyString = request<CommonParams, SDeviceParams>("Sending GetState request...", args, &Owner::requestGetState);
        } else if (cmd == ".prop") {
            replyString = request<CommonParams, SetPropertiesParams>("Sending SetProperties request...", args, &Owner::requestSetProperties);
        } else if (cmd == ".start") {
            replyString = request<CommonParams, SDeviceParams>("Sending Start request...", args, &Owner::requestStart);
        } else if (cmd == ".stop") {
            replyString = request<CommonParams, SDeviceParams>("Sending Stop request...", args, &Owner::requestStop);
        } else if (cmd == ".reset") {
            replyString = request<CommonParams, SDeviceParams>("Sending Reset request...", args, &Owner::requestReset);
        } else if (cmd == ".term") {
            replyString = request<CommonParams, SDeviceParams>("Sending Terminate request...", args, &Owner::requestTerminate);
        } else if (cmd == ".down") {
            replyString = request<CommonParams>("Sending Shutdown request...", args, &Owner::requestShutdown);
        } else if (cmd == ".status") {
            replyString = request<SStatusParams>("Sending Status request...", args, &Owner::requestStatus);
        } else if (cmd == ".batch") {
            execBatch(args);
        } else if (cmd == ".sleep") {
            execSleep(args);
        } else if (cmd == ".help") {
            printDescription();
        } else {
            if (cmd.length() > 0) {
                std::cout << "Unknown command " << _cmd << std::endl;
            }
        }

        if (!replyString.empty()) {
            std::cout << "Reply: (\n" << replyString << ")" << std::endl;
        }
    }

    void printDescription()
    {
        std::cout << "ODC Client.\n"
                  << "Each command has a set of extra options. Use " << std::quoted("<command> --help") << " to list available options.\n"
                  << "For example, " << std::quoted(".activate --topo topo_file.xml") << " command activates a topology " << std::quoted("topo_file.xml") << ".\n\n"
                  << "Available commands:\n\n"
                  << ".init - Initialize. Creates a new DDS session or attaches to an existing DDS session.\n"
                  << ".submit - Submit DDS agents. Can be called multiple times.\n"
                  << ".activate - Activates DDS topology (devices enter Idle state).\n"
                  << ".run - Combines Initialize, Submit and Activate commands. A new DDS session is always created.\n"
                  << ".prop - Set device properties.\n"
                  << ".upscale - Upscale topology.\n"
                  << ".downscale - Downscale topology.\n"
                  << ".state - Get current aggregated state of devices.\n"
                  << ".config - Transitions devices to Ready state (InitDevice->CompleteInit->Bind->Connect->InitTask).\n"
                  << ".start - Transitions devices to Running state (via Run transition).\n"
                  << ".stop - Transitions devices to Ready state (via Stop transition).\n"
                  << ".reset - Transitions devices to Idle state (via ResetTask->ResetDevice transitions).\n"
                  << ".term - Shutdown devices via End transition.\n"
                  << ".down - Shutdown DDS session.\n"
                  << ".status - Show statuses of managed partitions/sessions.\n"
                  << ".batch - Execute an array of commands.\n"
                  << ".sleep - Sleep for X ms.\n"
                  << ".help - Print available commands.\n"
                  << ".quit - Quit the program.\n" << std::endl;
    }
};
} // namespace odc::core

#endif /* defined(__ODC__CliServiceHelper__) */
