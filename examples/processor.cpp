/********************************************************************************
 * Copyright (C) 2014-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <fairlogger/Logger.h>
#include <fairmq/Device.h>
#include <fairmq/runDevice.h>

#include <cstdlib> // getenv
#include <chrono>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace bpo = boost::program_options;

struct Processor : fair::mq::Device
{
    Processor() { OnData("data1", &Processor::HandleData); }

    static void HangOrCrash(const std::vector<std::string>& problemPaths,
        const std::string& ddsTaskPath,
        const std::string& problemState,
        const std::string& currentState,
        const std::string& problem)
    {
        if (!problemPaths.empty() && !problemState.empty()) {
            if (problemState != currentState) {
                return;
            }

            bool crash = true;
            if (problem == "hang") {
                crash = false;
            } else if (problem == "crash") {
                crash = true;
            } else {
                return;
            }

            for (const auto& pathRegexStr : problemPaths) {
                std::regex pathRegex(pathRegexStr);
                if (std::regex_match(ddsTaskPath, pathRegex)) {
                    LOG(info) << "<<< task path " << std::quoted(ddsTaskPath) << " matches " << std::quoted(pathRegexStr)
                        << ", simulating a " << problem << ">>>";
                    if (crash) {
                        throw std::runtime_error("Instructed to crash. throwing runtime_error");
                    } else {
                        while (true) {
                            LOG(info) << "hanging...";
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                    }
                } else {
                    LOG(info) << "task path " << std::quoted(ddsTaskPath) << " does not match: " << std::quoted(pathRegexStr) << ".";
                }
            }
        }
    }

    void Init() override
    {
        GetConfig()->SetProperty<std::string>("crash", "no");
        GetConfig()->SubscribeAsString("ledevice", [](auto key, auto value) {
            if (key == "crash" && value == "yes") {
                LOG(warn) << "<<< CRASH >>>";
                std::abort();
            } else {
                LOG(info) << "property update: key: " << key << ", value: " << value;
            }
        });

        std::string ddsTaskPath(std::getenv("DDS_TASK_PATH"));
        LOG(info) << "DDS_TASK_PATH: " << ddsTaskPath;

        problemPaths = GetConfig()->GetProperty<std::vector<std::string>>("problem-paths", std::vector<std::string>());
        problem = GetConfig()->GetProperty<std::string>("problem", std::string());
        problemState = GetConfig()->GetProperty<std::string>("problem-state", std::string());

        HangOrCrash(problemPaths, ddsTaskPath, problemState, "init", problem);
    }

    bool HandleData(fair::mq::MessagePtr& msg, int)
    {
        // LOG(info) << "Received data, processing...";

        // Modify the received string
        std::string* text = new std::string(static_cast<char*>(msg->GetData()), msg->GetSize());
        *text += " (modified by " + fId + ")";

        fair::mq::MessagePtr msg2(NewMessage(const_cast<char*>(text->c_str()), text->length(), [](void* /*data*/, void* object) {
            delete static_cast<std::string*>(object);
        }, text));

        // Send out the output message
        if (Send(msg2, "data2") < 0) {
            return false;
        }

        return true;
    }

    std::vector<std::string> problemPaths;
    std::string problem;
    std::string problemState;
    bool selfDestruct = false;
    int taskIndex = 0;
    int collectionIndex = 0;
};

void addCustomOptions(bpo::options_description& options)
{
    options.add_options()
        ("problem-paths", bpo::value<std::vector<std::string>>()->multitoken()->composing(), "Set of topology paths for which task should hang/crash.")
        ("problem",       bpo::value<std::string>()->default_value("crash"), "Problem to simulate = crash/hang")
        ("problem-state", bpo::value<std::string>()->default_value(""), "State to simulate hanging/crashing in, default = no hanging. pre-idle/init");
}

std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& config) {
    std::string ddsTaskPath(std::getenv("DDS_TASK_PATH"));
    LOG(info) << "DDS_TASK_PATH: " << ddsTaskPath;

    std::vector<std::string> problemPaths = config.GetProperty<std::vector<std::string>>("problem-paths", std::vector<std::string>());
    std::string problem = config.GetProperty<std::string>("problem", std::string());
    std::string problemState = config.GetProperty<std::string>("problem-state", std::string());

    Processor::HangOrCrash(problemPaths, ddsTaskPath, problemState, "pre-idle", problem);

    return std::make_unique<Processor>();
}
