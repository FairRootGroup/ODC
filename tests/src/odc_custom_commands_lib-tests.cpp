/********************************************************************************
 * Copyright (C) 2019-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#define BOOST_TEST_MODULE(odc_custom_commands)
#define BOOST_TEST_DYN_LINK
#include <boost/test/included/unit_test.hpp>

#include "CustomCommands.h"
#include <string>

using namespace boost::unit_test;

using namespace odc::cc;
using namespace fair::mq;

BOOST_AUTO_TEST_SUITE(format);

BOOST_AUTO_TEST_CASE(construction)
{
    auto const props(std::vector<std::pair<std::string, std::string>>({{"k1", "v1"}, {"k2", "v2"}}));

    Cmds checkStateCmds(make<CheckState>());
    Cmds changeStateCmds(make<ChangeState>(fair::mq::Transition::Stop));
    Cmds dumpConfigCmds(make<DumpConfig>());
    Cmds subscribeToStateChangeCmds(make<SubscribeToStateChange>(60000));
    Cmds unsubscribeFromStateChangeCmds(make<UnsubscribeFromStateChange>());
    Cmds stateChangeExitingReceivedCmds(make<StateChangeExitingReceived>());
    Cmds getPropertiesCmds(make<GetProperties>(66, "k[12]"));
    Cmds setPropertiesCmds(make<SetProperties>(42, props));
    Cmds subscriptionHeartbeatCmds(make<SubscriptionHeartbeat>(60000));
    Cmds currentStateCmds(make<CurrentState>("somedeviceid", State::Running));
    Cmds transitionStatusCmds(make<TransitionStatus>("somedeviceid", 123456, Result::Ok, Transition::Stop, State::Running));
    Cmds configCmds(make<Config>("somedeviceid", "someconfig"));
    Cmds stateChangeSubscriptionCmds(make<StateChangeSubscription>("somedeviceid", 123456, Result::Ok));
    Cmds stateChangeUnsubscriptionCmds(make<StateChangeUnsubscription>("somedeviceid", 123456, Result::Ok));
    Cmds stateChangeCmds(make<StateChange>("somedeviceid", 123456, State::Running, State::Ready));
    Cmds propertiesCmds(make<Properties>("somedeviceid", 66, Result::Ok, props));
    Cmds propertiesSetCmds(make<PropertiesSet>("somedeviceid", 42, Result::Ok));

   BOOST_TEST(checkStateCmds.At(0).GetType() == Type::check_state);
   BOOST_TEST(changeStateCmds.At(0).GetType() == Type::change_state);
   BOOST_TEST(static_cast<ChangeState&>(changeStateCmds.At(0)).GetTransition() == Transition::Stop);
   BOOST_TEST(dumpConfigCmds.At(0).GetType() == Type::dump_config);
   BOOST_TEST(subscribeToStateChangeCmds.At(0).GetType() == Type::subscribe_to_state_change);
   BOOST_TEST(static_cast<SubscribeToStateChange&>(subscribeToStateChangeCmds.At(0)).GetInterval() == 60000);
   BOOST_TEST(unsubscribeFromStateChangeCmds.At(0).GetType() == Type::unsubscribe_from_state_change);
   BOOST_TEST(stateChangeExitingReceivedCmds.At(0).GetType() == Type::state_change_exiting_received);
   BOOST_TEST(getPropertiesCmds.At(0).GetType() == Type::get_properties);
   BOOST_TEST(static_cast<GetProperties&>(getPropertiesCmds.At(0)).GetRequestId() == 66);
   BOOST_TEST(static_cast<GetProperties&>(getPropertiesCmds.At(0)).GetQuery() == "k[12]");
   BOOST_TEST(setPropertiesCmds.At(0).GetType() == Type::set_properties);
   BOOST_TEST(static_cast<SetProperties&>(setPropertiesCmds.At(0)).GetRequestId() == 42);
   BOOST_TEST(static_cast<SetProperties&>(setPropertiesCmds.At(0)).GetProps() == props);
   BOOST_TEST(subscriptionHeartbeatCmds.At(0).GetType() == Type::subscription_heartbeat);
   BOOST_TEST(static_cast<SubscriptionHeartbeat&>(subscriptionHeartbeatCmds.At(0)).GetInterval() == 60000);
   BOOST_TEST(currentStateCmds.At(0).GetType() == Type::current_state);
   BOOST_TEST(static_cast<CurrentState&>(currentStateCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<CurrentState&>(currentStateCmds.At(0)).GetCurrentState() == State::Running);
   BOOST_TEST(transitionStatusCmds.At(0).GetType() == Type::transition_status);
   BOOST_TEST(static_cast<TransitionStatus&>(transitionStatusCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<TransitionStatus&>(transitionStatusCmds.At(0)).GetTaskId() == 123456);
   BOOST_TEST(static_cast<TransitionStatus&>(transitionStatusCmds.At(0)).GetResult() == Result::Ok);
   BOOST_TEST(static_cast<TransitionStatus&>(transitionStatusCmds.At(0)).GetTransition() == Transition::Stop);
   BOOST_TEST(static_cast<TransitionStatus&>(transitionStatusCmds.At(0)).GetCurrentState() == State::Running);
   BOOST_TEST(configCmds.At(0).GetType() == Type::config);
   BOOST_TEST(static_cast<Config&>(configCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<Config&>(configCmds.At(0)).GetConfig() == "someconfig");
   BOOST_TEST(stateChangeSubscriptionCmds.At(0).GetType() == Type::state_change_subscription);
   BOOST_TEST(static_cast<StateChangeSubscription&>(stateChangeSubscriptionCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<StateChangeSubscription&>(stateChangeSubscriptionCmds.At(0)).GetTaskId() == 123456);
   BOOST_TEST(static_cast<StateChangeSubscription&>(stateChangeSubscriptionCmds.At(0)).GetResult() == Result::Ok);
   BOOST_TEST(stateChangeUnsubscriptionCmds.At(0).GetType() == Type::state_change_unsubscription);
   BOOST_TEST(static_cast<StateChangeUnsubscription&>(stateChangeUnsubscriptionCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<StateChangeUnsubscription&>(stateChangeUnsubscriptionCmds.At(0)).GetTaskId() == 123456);
   BOOST_TEST(static_cast<StateChangeUnsubscription&>(stateChangeUnsubscriptionCmds.At(0)).GetResult() == Result::Ok);
   BOOST_TEST(stateChangeCmds.At(0).GetType() == Type::state_change);
   BOOST_TEST(static_cast<StateChange&>(stateChangeCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<StateChange&>(stateChangeCmds.At(0)).GetTaskId() == 123456);
   BOOST_TEST(static_cast<StateChange&>(stateChangeCmds.At(0)).GetLastState() == State::Running);
   BOOST_TEST(static_cast<StateChange&>(stateChangeCmds.At(0)).GetCurrentState() == State::Ready);
   BOOST_TEST(propertiesCmds.At(0).GetType() == Type::properties);
   BOOST_TEST(static_cast<Properties&>(propertiesCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<Properties&>(propertiesCmds.At(0)).GetRequestId() == 66);
   BOOST_TEST(static_cast<Properties&>(propertiesCmds.At(0)).GetResult() == Result::Ok);
   BOOST_TEST(static_cast<Properties&>(propertiesCmds.At(0)).GetProps() == props);
   BOOST_TEST(propertiesSetCmds.At(0).GetType() == Type::properties_set);
   BOOST_TEST(static_cast<PropertiesSet&>(propertiesSetCmds.At(0)).GetDeviceId() == "somedeviceid");
   BOOST_TEST(static_cast<PropertiesSet&>(propertiesSetCmds.At(0)).GetRequestId() == 42);
   BOOST_TEST(static_cast<PropertiesSet&>(propertiesSetCmds.At(0)).GetResult() == Result::Ok);
}

void fillCommands(Cmds& cmds)
{
    auto const props(std::vector<std::pair<std::string, std::string>>({{"k1", "v1"}, {"k2", "v2"}}));

    cmds.Add<CheckState>();
    cmds.Add<ChangeState>(Transition::Stop);
    cmds.Add<DumpConfig>();
    cmds.Add<SubscribeToStateChange>(60000);
    cmds.Add<UnsubscribeFromStateChange>();
    cmds.Add<StateChangeExitingReceived>();
    cmds.Add<GetProperties>(66, "k[12]");
    cmds.Add<SetProperties>(42, props);
    cmds.Add<SubscriptionHeartbeat>(60000);
    cmds.Add<CurrentState>("somedeviceid", State::Running);
    cmds.Add<TransitionStatus>("somedeviceid", 123456, Result::Ok, Transition::Stop, State::Running);
    cmds.Add<Config>("somedeviceid", "someconfig");
    cmds.Add<StateChangeSubscription>("somedeviceid", 123456, Result::Ok);
    cmds.Add<StateChangeUnsubscription>("somedeviceid", 123456, Result::Ok);
    cmds.Add<StateChange>("somedeviceid", 123456, State::Running, State::Ready);
    cmds.Add<Properties>("somedeviceid", 66, Result::Ok, props);
    cmds.Add<PropertiesSet>("somedeviceid", 42, Result::Ok);
}

void checkCommands(Cmds& cmds)
{
    BOOST_TEST(cmds.Size() == 17);

    int count = 0;
    auto const props(std::vector<std::pair<std::string, std::string>>({{"k1", "v1"}, {"k2", "v2"}}));

    for (const auto& cmd : cmds) {
        switch (cmd->GetType()) {
            case Type::check_state:
                ++count;
            break;
            case Type::change_state:
                ++count;
                BOOST_TEST(static_cast<ChangeState&>(*cmd).GetTransition() == Transition::Stop);
            break;
            case Type::dump_config:
                ++count;
            break;
            case Type::subscribe_to_state_change:
                ++count;
                BOOST_TEST(static_cast<SubscribeToStateChange&>(*cmd).GetInterval() == 60000);
            break;
            case Type::unsubscribe_from_state_change:
                ++count;
            break;
            case Type::state_change_exiting_received:
                ++count;
            break;
            case Type::get_properties:
                ++count;
                BOOST_TEST(static_cast<GetProperties&>(*cmd).GetRequestId() == 66);
                BOOST_TEST(static_cast<GetProperties&>(*cmd).GetQuery() == "k[12]");
            break;
            case Type::set_properties:
                ++count;
                BOOST_TEST(static_cast<SetProperties&>(*cmd).GetRequestId() == 42);
                BOOST_TEST(static_cast<SetProperties&>(*cmd).GetProps() == props);
            break;
            case Type::subscription_heartbeat:
                ++count;
                BOOST_TEST(static_cast<SubscriptionHeartbeat&>(*cmd).GetInterval() == 60000);
            break;
            case Type::current_state:
                ++count;
                BOOST_TEST(static_cast<CurrentState&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<CurrentState&>(*cmd).GetCurrentState() == State::Running);
            break;
            case Type::transition_status:
                ++count;
                BOOST_TEST(static_cast<TransitionStatus&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<TransitionStatus&>(*cmd).GetTaskId() == 123456);
                BOOST_TEST(static_cast<TransitionStatus&>(*cmd).GetResult() == Result::Ok);
                BOOST_TEST(static_cast<TransitionStatus&>(*cmd).GetTransition() == Transition::Stop);
                BOOST_TEST(static_cast<TransitionStatus&>(*cmd).GetCurrentState() == State::Running);
            break;
            case Type::config:
                ++count;
                BOOST_TEST(static_cast<Config&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<Config&>(*cmd).GetConfig() == "someconfig");
            break;
            case Type::state_change_subscription:
                ++count;
                BOOST_TEST(static_cast<StateChangeSubscription&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<StateChangeSubscription&>(*cmd).GetTaskId() == 123456);
                BOOST_TEST(static_cast<StateChangeSubscription&>(*cmd).GetResult() == Result::Ok);
            break;
            case Type::state_change_unsubscription:
                ++count;
                BOOST_TEST(static_cast<StateChangeUnsubscription&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<StateChangeUnsubscription&>(*cmd).GetTaskId() == 123456);
                BOOST_TEST(static_cast<StateChangeUnsubscription&>(*cmd).GetResult() == Result::Ok);
            break;
            case Type::state_change:
                ++count;
                BOOST_TEST(static_cast<StateChange&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<StateChange&>(*cmd).GetTaskId() == 123456);
                BOOST_TEST(static_cast<StateChange&>(*cmd).GetLastState() == State::Running);
                BOOST_TEST(static_cast<StateChange&>(*cmd).GetCurrentState() == State::Ready);
            break;
            case Type::properties:
                ++count;
                BOOST_TEST(static_cast<Properties&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<Properties&>(*cmd).GetRequestId() == 66);
                BOOST_TEST(static_cast<Properties&>(*cmd).GetResult() == Result::Ok);
                BOOST_TEST(static_cast<Properties&>(*cmd).GetProps() == props);
            break;
            case Type::properties_set:
                ++count;
                BOOST_TEST(static_cast<PropertiesSet&>(*cmd).GetDeviceId() == "somedeviceid");
                BOOST_TEST(static_cast<PropertiesSet&>(*cmd).GetRequestId() == 42);
                BOOST_TEST(static_cast<PropertiesSet&>(*cmd).GetResult() == Result::Ok);
            break;
            default:
                BOOST_TEST(false);
            break;
        }
    }

    BOOST_TEST(count == 17);
}

BOOST_AUTO_TEST_CASE(serialization_binary)
{
    Cmds outCmds;
    fillCommands(outCmds);
    std::string buffer(outCmds.Serialize());

    Cmds inCmds;
    inCmds.Deserialize(buffer);
    checkCommands(inCmds);
}

BOOST_AUTO_TEST_CASE(serialization_json)
{
    Cmds outCmds;
    fillCommands(outCmds);
    std::string buffer(outCmds.Serialize(Format::JSON));

    Cmds inCmds;
    inCmds.Deserialize(buffer, Format::JSON);
    checkCommands(inCmds);
}

BOOST_AUTO_TEST_SUITE_END()
