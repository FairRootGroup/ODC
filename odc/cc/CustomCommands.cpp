/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <odc/cc/CustomCommands.h>
#include <odc/cc/CustomCommandsFormat.h>
#include <odc/cc/CustomCommandsFormatDef.h>

#include <flatbuffers/idl.h>

#include <array>

using namespace std;

namespace odc::cc
{

    array<Result, 2> fbResultToResult = { { Result::Ok, Result::Failure } };

    array<FBResult, 2> resultToFBResult = { { FBResult::FBResult_Ok, FBResult::FBResult_Failure } };

    array<string, 2> resultNames = { { "Ok", "Failure" } };

    array<string, 15> typeNames = { { "CheckState",
                                      "ChangeState",
                                      "DumpConfig",
                                      "SubscribeToStateChange",
                                      "UnsubscribeFromStateChange",
                                      "GetProperties",
                                      "SetProperties",
                                      "SubscriptionHeartbeat",

                                      "TransitionStatus",
                                      "Config",
                                      "StateChangeSubscription",
                                      "StateChangeUnsubscription",
                                      "StateChange",
                                      "Properties",
                                      "PropertiesSet" } };

    array<fair::mq::State, 16> fbStateToMQState = { { fair::mq::State::Undefined,
                                                      fair::mq::State::Ok,
                                                      fair::mq::State::Error,
                                                      fair::mq::State::Idle,
                                                      fair::mq::State::InitializingDevice,
                                                      fair::mq::State::Initialized,
                                                      fair::mq::State::Binding,
                                                      fair::mq::State::Bound,
                                                      fair::mq::State::Connecting,
                                                      fair::mq::State::DeviceReady,
                                                      fair::mq::State::InitializingTask,
                                                      fair::mq::State::Ready,
                                                      fair::mq::State::Running,
                                                      fair::mq::State::ResettingTask,
                                                      fair::mq::State::ResettingDevice,
                                                      fair::mq::State::Exiting } };

    array<FBState, 16> mqStateToFBState = { { FBState_Undefined,
                                              FBState_Ok,
                                              FBState_Error,
                                              FBState_Idle,
                                              FBState_InitializingDevice,
                                              FBState_Initialized,
                                              FBState_Binding,
                                              FBState_Bound,
                                              FBState_Connecting,
                                              FBState_DeviceReady,
                                              FBState_InitializingTask,
                                              FBState_Ready,
                                              FBState_Running,
                                              FBState_ResettingTask,
                                              FBState_ResettingDevice,
                                              FBState_Exiting } };

    array<fair::mq::Transition, 12> fbTransitionToMQTransition = { { fair::mq::Transition::Auto,
                                                                     fair::mq::Transition::InitDevice,
                                                                     fair::mq::Transition::CompleteInit,
                                                                     fair::mq::Transition::Bind,
                                                                     fair::mq::Transition::Connect,
                                                                     fair::mq::Transition::InitTask,
                                                                     fair::mq::Transition::Run,
                                                                     fair::mq::Transition::Stop,
                                                                     fair::mq::Transition::ResetTask,
                                                                     fair::mq::Transition::ResetDevice,
                                                                     fair::mq::Transition::End,
                                                                     fair::mq::Transition::ErrorFound } };

    array<FBTransition, 12> mqTransitionToFBTransition = { { FBTransition_Auto,
                                                             FBTransition_InitDevice,
                                                             FBTransition_CompleteInit,
                                                             FBTransition_Bind,
                                                             FBTransition_Connect,
                                                             FBTransition_InitTask,
                                                             FBTransition_Run,
                                                             FBTransition_Stop,
                                                             FBTransition_ResetTask,
                                                             FBTransition_ResetDevice,
                                                             FBTransition_End,
                                                             FBTransition_ErrorFound } };

    array<FBCmd, 15> typeToFBCmd = { { FBCmd::FBCmd_check_state,
                                       FBCmd::FBCmd_change_state,
                                       FBCmd::FBCmd_dump_config,
                                       FBCmd::FBCmd_subscribe_to_state_change,
                                       FBCmd::FBCmd_unsubscribe_from_state_change,
                                       FBCmd::FBCmd_get_properties,
                                       FBCmd::FBCmd_set_properties,
                                       FBCmd::FBCmd_subscription_heartbeat,
                                       FBCmd::FBCmd_transition_status,
                                       FBCmd::FBCmd_config,
                                       FBCmd::FBCmd_state_change_subscription,
                                       FBCmd::FBCmd_state_change_unsubscription,
                                       FBCmd::FBCmd_state_change,
                                       FBCmd::FBCmd_properties,
                                       FBCmd::FBCmd_properties_set } };

    array<Type, 15> fbCmdToType = { { Type::check_state,
                                      Type::change_state,
                                      Type::dump_config,
                                      Type::subscribe_to_state_change,
                                      Type::unsubscribe_from_state_change,
                                      Type::get_properties,
                                      Type::set_properties,
                                      Type::subscription_heartbeat,
                                      Type::transition_status,
                                      Type::config,
                                      Type::state_change_subscription,
                                      Type::state_change_unsubscription,
                                      Type::state_change,
                                      Type::properties,
                                      Type::properties_set } };

    fair::mq::State GetMQState(const FBState state)
    {
        return fbStateToMQState.at(state);
    }
    FBState GetFBState(const fair::mq::State state)
    {
        return mqStateToFBState.at(static_cast<int>(state));
    }
    fair::mq::Transition GetMQTransition(const FBTransition transition)
    {
        return fbTransitionToMQTransition.at(transition);
    }
    FBTransition GetFBTransition(const fair::mq::Transition transition)
    {
        return mqTransitionToFBTransition.at(static_cast<int>(transition));
    }

    Result GetResult(const FBResult result)
    {
        return fbResultToResult.at(result);
    }
    FBResult GetFBResult(const Result result)
    {
        return resultToFBResult.at(static_cast<int>(result));
    }
    string GetResultName(const Result result)
    {
        return resultNames.at(static_cast<int>(result));
    }
    string GetTypeName(const Type type)
    {
        return typeNames.at(static_cast<int>(type));
    }

    FBCmd GetFBCmd(const Type type)
    {
        return typeToFBCmd.at(static_cast<int>(type));
    }

    string Cmds::Serialize() const
    {
        flatbuffers::FlatBufferBuilder fbb;
        vector<flatbuffers::Offset<FBCommand>> commandOffsets;

        for (auto& cmd : fCmds)
        {
            flatbuffers::Offset<FBCommand> cmdOffset;
            unique_ptr<FBCommandBuilder> cmdBuilder; // delay the creation of the builder, because child strings need to
                                                     // be constructed first (which are conditional)

            switch (cmd->GetType())
            {
                case Type::check_state:
                {
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                }
                break;
                case Type::change_state:
                {
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_transition(GetFBTransition(static_cast<ChangeState&>(*cmd).GetTransition()));
                }
                break;
                case Type::dump_config:
                {
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                }
                break;
                    break;
                case Type::subscribe_to_state_change:
                {
                    auto _cmd = static_cast<SubscribeToStateChange&>(*cmd);
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_interval(_cmd.GetInterval());
                }
                break;
                case Type::unsubscribe_from_state_change:
                {
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                }
                break;
                case Type::get_properties:
                {
                    auto _cmd = static_cast<GetProperties&>(*cmd);
                    auto query = fbb.CreateString(_cmd.GetQuery());
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_request_id(_cmd.GetRequestId());
                    cmdBuilder->add_property_query(query);
                }
                break;
                case Type::set_properties:
                {
                    auto _cmd = static_cast<SetProperties&>(*cmd);
                    std::vector<flatbuffers::Offset<FBProperty>> propsVector;
                    for (auto const& e : _cmd.GetProps())
                    {
                        auto const key(fbb.CreateString(e.first));
                        auto const val(fbb.CreateString(e.second));
                        propsVector.push_back(CreateFBProperty(fbb, key, val));
                    }
                    auto props = fbb.CreateVector(propsVector);
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_request_id(_cmd.GetRequestId());
                    cmdBuilder->add_properties(props);
                }
                break;
                case Type::subscription_heartbeat:
                {
                    auto _cmd = static_cast<SubscriptionHeartbeat&>(*cmd);
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_interval(_cmd.GetInterval());
                }
                break;
                case Type::transition_status:
                {
                    auto _cmd = static_cast<TransitionStatus&>(*cmd);
                    auto deviceId = fbb.CreateString(_cmd.GetDeviceId());
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_device_id(deviceId);
                    cmdBuilder->add_task_id(_cmd.GetTaskId());
                    cmdBuilder->add_result(GetFBResult(_cmd.GetResult()));
                    cmdBuilder->add_transition(GetFBTransition(_cmd.GetTransition()));
                    cmdBuilder->add_current_state(GetFBState(_cmd.GetCurrentState()));
                }
                break;
                case Type::config:
                {
                    auto _cmd = static_cast<Config&>(*cmd);
                    auto deviceId = fbb.CreateString(_cmd.GetDeviceId());
                    auto config = fbb.CreateString(_cmd.GetConfig());
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_device_id(deviceId);
                    cmdBuilder->add_config_string(config);
                }
                break;
                case Type::state_change_subscription:
                {
                    auto _cmd = static_cast<StateChangeSubscription&>(*cmd);
                    auto deviceId = fbb.CreateString(_cmd.GetDeviceId());
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_device_id(deviceId);
                    cmdBuilder->add_task_id(_cmd.GetTaskId());
                    cmdBuilder->add_result(GetFBResult(_cmd.GetResult()));
                }
                break;
                case Type::state_change_unsubscription:
                {
                    auto _cmd = static_cast<StateChangeUnsubscription&>(*cmd);
                    auto deviceId = fbb.CreateString(_cmd.GetDeviceId());
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_device_id(deviceId);
                    cmdBuilder->add_task_id(_cmd.GetTaskId());
                    cmdBuilder->add_result(GetFBResult(_cmd.GetResult()));
                }
                break;
                case Type::state_change:
                {
                    auto _cmd = static_cast<StateChange&>(*cmd);
                    auto deviceId = fbb.CreateString(_cmd.GetDeviceId());
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_device_id(deviceId);
                    cmdBuilder->add_task_id(_cmd.GetTaskId());
                    cmdBuilder->add_last_state(GetFBState(_cmd.GetLastState()));
                    cmdBuilder->add_current_state(GetFBState(_cmd.GetCurrentState()));
                }
                break;
                case Type::properties:
                {
                    auto _cmd = static_cast<Properties&>(*cmd);
                    auto deviceId = fbb.CreateString(_cmd.GetDeviceId());

                    std::vector<flatbuffers::Offset<FBProperty>> propsVector;
                    for (const auto& e : _cmd.GetProps())
                    {
                        auto key = fbb.CreateString(e.first);
                        auto val = fbb.CreateString(e.second);
                        auto prop = CreateFBProperty(fbb, key, val);
                        propsVector.push_back(prop);
                    }
                    auto props = fbb.CreateVector(propsVector);
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_device_id(deviceId);
                    cmdBuilder->add_task_id(_cmd.GetTaskId());
                    cmdBuilder->add_request_id(_cmd.GetRequestId());
                    cmdBuilder->add_result(GetFBResult(_cmd.GetResult()));
                    cmdBuilder->add_properties(props);
                }
                break;
                case Type::properties_set:
                {
                    auto _cmd = static_cast<PropertiesSet&>(*cmd);
                    auto deviceId = fbb.CreateString(_cmd.GetDeviceId());
                    cmdBuilder = make_unique<FBCommandBuilder>(fbb);
                    cmdBuilder->add_device_id(deviceId);
                    cmdBuilder->add_task_id(_cmd.GetTaskId());
                    cmdBuilder->add_request_id(_cmd.GetRequestId());
                    cmdBuilder->add_result(GetFBResult(_cmd.GetResult()));
                }
                break;
                default:
                    throw CommandFormatError("unrecognized command type given to odc::cc::Cmds::Serialize()");
                    break;
            }

            cmdBuilder->add_command_id(typeToFBCmd.at(static_cast<int>(cmd->GetType())));

            cmdOffset = cmdBuilder->Finish();
            commandOffsets.push_back(cmdOffset);
        }

        auto commands = fbb.CreateVector(commandOffsets);
        auto cmds = CreateFBCommands(fbb, commands);
        fbb.Finish(cmds);

        return string(reinterpret_cast<char*>(fbb.GetBufferPointer()), fbb.GetSize());
    }

    void Cmds::Deserialize(const string& str)
    {
        fCmds.clear();

        const flatbuffers::Vector<flatbuffers::Offset<FBCommand>>* cmds = nullptr;

        flatbuffers::Parser parser;

        cmds = GetFBCommands(const_cast<char*>(str.c_str()))->commands();

        for (unsigned int i = 0; i < cmds->size(); ++i)
        {
            const FBCommand& cmdPtr = *(cmds->Get(i));
            switch (cmdPtr.command_id())
            {
                case FBCmd_check_state:
                    fCmds.emplace_back(make<CheckState>());
                    break;
                case FBCmd_change_state:
                    fCmds.emplace_back(make<ChangeState>(GetMQTransition(cmdPtr.transition())));
                    break;
                case FBCmd_dump_config:
                    fCmds.emplace_back(make<DumpConfig>());
                    break;
                case FBCmd_subscribe_to_state_change:
                    fCmds.emplace_back(make<SubscribeToStateChange>(cmdPtr.interval()));
                    break;
                case FBCmd_unsubscribe_from_state_change:
                    fCmds.emplace_back(make<UnsubscribeFromStateChange>());
                    break;
                case FBCmd_get_properties:
                    fCmds.emplace_back(make<GetProperties>(cmdPtr.request_id(), cmdPtr.property_query()->str()));
                    break;
                case FBCmd_set_properties:
                {
                    std::vector<std::pair<std::string, std::string>> properties;
                    auto props = cmdPtr.properties();
                    for (unsigned int j = 0; j < props->size(); ++j)
                    {
                        properties.emplace_back(props->Get(j)->key()->str(), props->Get(j)->value()->str());
                    }
                    fCmds.emplace_back(make<SetProperties>(cmdPtr.request_id(), properties));
                }
                break;
                case FBCmd_subscription_heartbeat:
                    fCmds.emplace_back(make<SubscriptionHeartbeat>(cmdPtr.interval()));
                    break;
                case FBCmd_transition_status:
                    fCmds.emplace_back(make<TransitionStatus>(cmdPtr.device_id()->str(),
                                                              cmdPtr.task_id(),
                                                              GetResult(cmdPtr.result()),
                                                              GetMQTransition(cmdPtr.transition()),
                                                              GetMQState(cmdPtr.current_state())));
                    break;
                case FBCmd_config:
                    fCmds.emplace_back(make<Config>(cmdPtr.device_id()->str(), cmdPtr.config_string()->str()));
                    break;
                case FBCmd_state_change_subscription:
                    fCmds.emplace_back(make<StateChangeSubscription>(
                        cmdPtr.device_id()->str(), cmdPtr.task_id(), GetResult(cmdPtr.result())));
                    break;
                case FBCmd_state_change_unsubscription:
                    fCmds.emplace_back(make<StateChangeUnsubscription>(
                        cmdPtr.device_id()->str(), cmdPtr.task_id(), GetResult(cmdPtr.result())));
                    break;
                case FBCmd_state_change:
                    fCmds.emplace_back(make<StateChange>(cmdPtr.device_id()->str(),
                                                         cmdPtr.task_id(),
                                                         GetMQState(cmdPtr.last_state()),
                                                         GetMQState(cmdPtr.current_state())));
                    break;
                case FBCmd_properties:
                {
                    std::vector<std::pair<std::string, std::string>> properties;
                    auto props = cmdPtr.properties();
                    for (unsigned int j = 0; j < props->size(); ++j)
                    {
                        properties.emplace_back(props->Get(j)->key()->str(), props->Get(j)->value()->str());
                    }
                    fCmds.emplace_back(make<Properties>(cmdPtr.device_id()->str(),
                                                        cmdPtr.task_id(),
                                                        cmdPtr.request_id(),
                                                        GetResult(cmdPtr.result()),
                                                        properties));
                }
                break;
                case FBCmd_properties_set:
                    fCmds.emplace_back(make<PropertiesSet>(
                        cmdPtr.device_id()->str(), cmdPtr.task_id(), cmdPtr.request_id(), GetResult(cmdPtr.result())));
                    break;
                default:
                    throw CommandFormatError("unrecognized command type given to odc::cc::Cmds::Deserialize()");
                    break;
            }
        }
    }

} // namespace odc::cc
