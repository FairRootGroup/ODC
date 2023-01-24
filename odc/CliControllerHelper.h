/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CLICONTROLLERHELPER
#define ODC_CLICONTROLLERHELPER

#include <odc/CliHelper.h>
#include <odc/Params.h>
#include <odc/Logger.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#ifdef READLINE_AVAIL
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include <iostream>
#include <thread>
#include <tuple>

namespace bpo = boost::program_options;

namespace odc::core {

template<typename Owner>
class CliControllerHelper
{
  public:
    /// \brief Run the service
    /// \param[in] cmds Array of requests. If empty than command line input is required.
    void run(const std::vector<std::string>& cmds = std::vector<std::string>())
    {
        std::cout << "ODC Client. Use \".help\" to list available commands." << std::endl;

        // Read the input from commnad line
        if (cmds.empty()) {
#ifdef READLINE_AVAIL
            // Register command completion handler
            rl_attempted_completion_function = &CliControllerHelper::commandCompleter;
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
                std::cout << "Please enter command: " << std::endl;
                getline(std::cin, cmd);
#endif

                boost::trim_right(cmd);

                processRequest(cmd);
            }
        } else {
            // Execute consequently all commands
            execCmds(cmds);
            // Exit at the end
            exit(EXIT_SUCCESS);
        }
    }

  private:
#ifdef READLINE_AVAIL
    static char* commandGenerator(const char* text, int index)
    {
        static const std::vector<std::string> commands {
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
            return rl_completion_matches(text, &CliControllerHelper::commandGenerator);
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

    void execBatch(const std::vector<std::string>& args)
    {
        CliHelper::BatchOptions bopt;
        if (parseCommand(args, bopt)) {
            execCmds(bopt.mOutputCmds);
        }
    }

    void execSleep(const std::vector<std::string>& args)
    {
        try {
            CliHelper::SleepOptions sopt;
            if (parseCommand(args, sopt)) {
                if (sopt.mMs > 0) {
                    std::cout << "Sleeping " << sopt.mMs << " ms" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(sopt.mMs));
                }
            }
        } catch(const std::exception& e) {
            std::cout << "Error parsing command: " << e.what() << std::endl;
        }
    }

    template<typename... RequestParams>
    bool parseCommand(const std::vector<std::string>& args, RequestParams&&... params)
    {
        bpo::options_description options("Request options");
        options.add_options()("help,h", "Print help");

        // Loop over input parameters and add program options
        std::apply([&options](auto&&... par) { ((CliHelper::addOptions(options, par)), ...); }, std::tie(params...));

        bpo::variables_map vm;
        bpo::store(bpo::command_line_parser(args).options(options).run(), vm);
        bpo::notify(vm);

        std::apply([&vm](auto&&... par) { ((CliHelper::parseOptions(vm, par)), ...); }, std::tie(params...));

        if (vm.count("help")) {
            std::cout << options << std::endl;
            return false;
        }

        return true;
    }

    template<typename T>
    void print(const T& value) { std::cout << value << std::endl; }

    template<typename... RequestParams, typename Func>
    std::string request(const std::string& name, const std::vector<std::string>& args, Func func, RequestParams&&... params)
    {
        std::string result;
        try {
            if (parseCommand(args, params...)) {
                std::cout << "Sending " << name << " request..." << std::endl;
                std::apply([this](auto&&... par) { ((this->print(par)), ...); }, std::tie(params...));
                Owner* p = reinterpret_cast<Owner*>(this);
                result = (p->*func)(params...);
            }
        } catch(const std::exception& e) {
            std::cout << "Error parsing command: " << e.what() << std::endl;
        }
        return result;
    }

    void processRequest(const std::string& command)
    {
        if (command == ".quit") {
            exit(EXIT_SUCCESS);
        }

        std::string reply;
        std::vector<std::string> args{ bpo::split_unix(command) };
        std::string cmd{ args.empty() ? "" : args.front() };

        if (cmd == ".init") {
            reply = request("Initialize",    args, &Owner::requestInitialize,    CommonParams(), InitializeParams());
        } else if (cmd == ".submit") {
            reply = request("Submit",        args, &Owner::requestSubmit,        CommonParams(), SubmitParams());
        } else if (cmd == ".activate") {
            reply = request("Activate",      args, &Owner::requestActivate,      CommonParams(), ActivateParams());
        } else if (cmd == ".run") {
            reply = request("Run",           args, &Owner::requestRun,           CommonParams(), RunParams());
        } else if (cmd == ".upscale") {
            reply = request("Upscale",       args, &Owner::requestUpscale,       CommonParams(), UpdateParams());
        } else if (cmd == ".downscale") {
            reply = request("Downscale",     args, &Owner::requestDownscale,     CommonParams(), UpdateParams());
        } else if (cmd == ".config") {
            reply = request("Configure",     args, &Owner::requestConfigure,     CommonParams(), DeviceParams());
        } else if (cmd == ".state") {
            reply = request("GetState",      args, &Owner::requestGetState,      CommonParams(), DeviceParams());
        } else if (cmd == ".prop") {
            reply = request("SetProperties", args, &Owner::requestSetProperties, CommonParams(), SetPropertiesParams());
        } else if (cmd == ".start") {
            reply = request("Start",         args, &Owner::requestStart,         CommonParams(), DeviceParams());
        } else if (cmd == ".stop") {
            reply = request("Stop",          args, &Owner::requestStop,          CommonParams(), DeviceParams());
        } else if (cmd == ".reset") {
            reply = request("Reset",         args, &Owner::requestReset,         CommonParams(), DeviceParams());
        } else if (cmd == ".term") {
            reply = request("Terminate",     args, &Owner::requestTerminate,     CommonParams(), DeviceParams());
        } else if (cmd == ".down") {
            reply = request("Shutdown",      args, &Owner::requestShutdown,      CommonParams());
        } else if (cmd == ".status") {
            reply = request("Status",        args, &Owner::requestStatus,        StatusParams());
        } else if (cmd == ".batch") {
            execBatch(args);
        } else if (cmd == ".sleep") {
            execSleep(args);
        } else if (cmd == ".help") {
            printDescription();
        } else {
            if (cmd.length() > 0) {
                std::cout << "Unknown command " << command << std::endl;
            }
        }

        if (!reply.empty()) {
            std::cout << "Reply: (\n" << reply << ")" << std::endl;
        }
    }

    void printDescription()
    {
        std::cout << "Available commands:\n\n"

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
                  << ".help - List available commands.\n"
                  << ".quit - Quit the program.\n\n"

                  << "View command options with \"<command> --help\"" << std::endl;
    }
};
} // namespace odc::core

#endif /* defined(ODC_CLICONTROLLERHELPER) */
