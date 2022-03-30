/********************************************************************************
 * Copyright (C) 2014-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <fairlogger/Logger.h>
#include <fairmq/Device.h>
#include <fairmq/runDevice.h>

#include <cstdlib>
#include <string>

namespace bpo = boost::program_options;

struct Processor : fair::mq::Device
{
    Processor() { OnData("data1", &Processor::HandleData); }

    void Init() override
    {
        GetConfig()->SetProperty<std::string>("crash", "no");
        GetConfig()->SubscribeAsString("device", [](auto key, auto value) {
            if (key == "crash" && value == "yes") {
                LOG(warn) << "<<< CRASH >>>";
                std::abort();
            }
        });

        selfDestruct = GetConfig()->GetProperty<bool>("self-destruct", false);
        collectionIndex = GetConfig()->GetProperty<int>("collection-index", 0);
        LOG(warn) << "collectionIndex == " << collectionIndex << " and --self-destruct == " << selfDestruct;
        if (selfDestruct && collectionIndex % 2 != 0) {
            LOG(warn) << "<<< collectionIndex == " << collectionIndex << " and --self-destruct is true, aborting >>>";
            std::abort();
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

    bool selfDestruct = false;
    int collectionIndex = 0;
};

void addCustomOptions(bpo::options_description& options)
{
    options.add_options()("self-destruct", bpo::value<bool>()->default_value(false), "If true, abort() during Init()");
    options.add_options()("collection-index", bpo::value<int>()->default_value(0), "DDS collection index");
}

std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/) { return std::make_unique<Processor>(); }
