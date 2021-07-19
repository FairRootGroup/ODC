# Request Triggers

Request trigger is an external executable which can be registered and started whenever a particular request is processed. It can be written in any programming language and doesn’t depend on ODC.

The contract between request trigger and ODC:
 * **Input:** a description string. For example, it can be in JSON ar XML format or any other format. ODC executes plugin and provides a resource string via `--res`  and Partition ID via `--id` command line options:
```
myrt --res "Description string" --id "Partition ID"
```

### Register request trigger

In order to resource trigger one need to register it in ODC server. `odc-grpc-server` and `odc-cli-server` have `--rt` command line option which allows to register resource triggers. For example:
```bash
odc-grpc-server --rt Initialize:/path/to/my_rt Shutdown:/path/to/another_rt
```
`Initialize` and `Shutdown` correspond to request name and can be one of the following : `Initialize`, `Submit`, `Activate`, `Run`, `Update`, `Configure`, `SetProperties`, `GetState`, `Start`, `Stop`, `Reset`, `Terminate`, `Shutdown`, `Status`. 
