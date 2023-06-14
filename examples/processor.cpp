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
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace bpo = boost::program_options;

struct Processor : fair::mq::Device
{
    Processor() { OnData("data1", &Processor::HandleData); }

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

        selfDestructPaths = GetConfig()->GetProperty<std::vector<std::string>>("self-destruct-paths", std::vector<std::string>());
        if (!selfDestructPaths.empty()) {
            for (const auto& pathRegexStr : selfDestructPaths) {
                std::regex pathRegex(pathRegexStr);
                if (std::regex_match(ddsTaskPath, pathRegex)) {
                    LOG(info) << "<<< task path " << std::quoted(ddsTaskPath) << " matches " << std::quoted(pathRegexStr) << ", aborting >>>";
                    throw std::runtime_error("Instructed to self-destruct. throwing runtime_error");
                } else {
                    LOG(info) << "task path " << std::quoted(ddsTaskPath) << " does not match: " << std::quoted(pathRegexStr) << ".";
                }
            }
        }
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

    std::vector<int> selfDestructTaskIdx;
    std::vector<int> selfDestructCollectionIdx;
    std::vector<std::string> selfDestructPaths;
    bool selfDestruct = false;
    int taskIndex = 0;
    int collectionIndex = 0;
};

void addCustomOptions(bpo::options_description& options)
{
    options.add_options()
        ("self-destruct-paths", bpo::value<std::vector<std::string>>()->multitoken()->composing(), "Set of topology paths for which task should destroy itself.");
}

std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/) { return std::make_unique<Processor>(); }
