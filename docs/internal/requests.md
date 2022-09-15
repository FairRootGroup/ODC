### Requests

| Request       | Oerations with a timeout                                                    |
| ------------- | --------------------------------------------------------------------------- |
| Initialize    | syncSendRequest\<SCommanderInfoRequest>                                     |
| Submit        | submitDDSAgents() -> waitOnDone()                                           |
| Submit        | ddsSession->waitForNumSlots\<dds::tools_api::CSession::EAgentState::active> |
| Submit        | getAgentInfo() -> syncSendRequest<SAgentInfoRequest>                        |
| Submit        | attemptSubmitRecovery() -> syncSendRequest<SAgentCountRequest>              |
| Submit        | DDSSubmit::makeParams() ->execPlugin() -> execute(30)                       |
| Activate      | activateDDSTopology(ACTIVATE) -> waitingOnDone()                            |
| Activate      | waitForState(Idle) -> mTopology->WaitForState                               |
| Activate      | topoFilepath() -> execute()                                                 |
| Run           | submitDDSAgents() -> waitOnDone()                                           |
| Run           | ddsSession->waitForNumSlots\<dds::tools_api::CSession::EAgentState::active> |
| Run           | getAgentInfo() -> syncSendRequest\<SAgentInfoRequest>                       |
| Run           | attemptSubmitRecovery() -> syncSendRequest\<SAgentCountRequest>             |
| Run           | DDSSubmit::makeParams() -> execPlugin() -> execute(30)                      |
| Run           | activateDDSTopology -> waitOnDone()                                         |
| Run           | waitForState(Idle) -> mTopology->WaitForState                               |
| Run           | topoFilepath() -> execute()                                                 |
| Update        | topoFilepath() -> execute()                                                 |
| Update        | activateDDSTopology(UPDATE) -> waitOnDone()                                 |
| Update        | mTopology->changeState (x n)                                                |
| Update        | mTopology->waitForState (x m)                                               |
| Configure     | mTopology->changeState (x 5)                                                |
| SetProperties | mTopology->SetProperties()                                                  |
| GetState      | -                                                                           |
| Start         | mTopology->ChangeState                                                      |
| Stop          | mTopology->ChangeState                                                      |
| Reset         | mTopology->changeState (x 2)                                                |
| Terminate     | mTopology->ChangeState                                                      |
| Shutdown      | -                                                                           |
| Status        | -                                                                           |
