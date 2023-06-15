# ODC Release Notes

## 0.78.0 (2023-06-15)

- Fixed known issue from 0.78.0-beta: agents are now shut down failed tasks/collections from expendable/nMin triggers.
- Bugfix: updated Topology Ops to allow ignoring of tasks. Used during ignore events in Topology, to avoid command races.

## 0.78.0-beta.2 (2023-05-19)

- InfoLogger: Set message levels (same translation as in AliceO2)

## 0.78.0-beta (2023-04-28)

- New Feature: Async handling for nMin and expendable tasks. Async GetState requests report proper state, taking expendable tasks & nMin into account.
- New Feature: Allow resource extraction from the topology file.

  Derives number of agents, agent slots and other requirements from the given topology file.

  To use this:
    1. Launch the server (grpc or cli) with `--rms <slurm/ssh/localhost>` and `--zones <name>:<cfgFilePath>:<envFilePath>` (latter is the same as --zones for the Slurm plugin, but without the number of slots).
    2. Set `extractTopoResources` to `true` in the Run request (or --extract-topo-resources true for CLI clients). Off by default.
    3. The Run request will not use the `plugin` and `resources` parameters. These can be left empty.
    4. For the server, `--rp` is not used. It doesn't have to be set.

  This is completely optional - previous approach works as before.

- **Breaking Change**: Remove request triggers (unused).
- **Breaking Change**: nMin of 0 does nothing in the main Controller. Previously empty group would be allowed to be executed. In practice this was unused because topology creation tools did not allow for this case.
- Bugfix: Change-/WaitForState Operation: handle unexpected exiting state
- Bugfix: WaitForState Operation: fix timer cancellation
- Bugfix: Topology: Include exited tasks when resetting op count
- Tests: Extend unit tests
- Tests: Add parameter test for epn topo
- Tests: Add testsuite for requirements tests
- Tests: Add testsuite for parameter tests

Known Issues:
- DDS agents are not shut down if their tasks/collections fail, but the session is still ongoing.

## 0.77.1 (2023-03-07)

- Bugfix: handle return value of FairMQ's ChangeState

## 0.77.0 (2023-02-20)

- Improvement: Fail early on ressource plugin failures.
- Improvement: Include agent startup time in the agent info output.
- Bugfix: Fix incorrect slot count on submission failures.
- Bugfix: Fix some CLI tools/commands not stopping execution after --help.
- Remove async mode of the gRPC controller. The `--sync` CLI parameter now does nothing and is **deprecated**.
- Remove logger from the odc-rp-epn-slurm plugin. `--logdir`, `--severity` and `--infologger` parameters now do nothing and are **deprecated**.
- Include list of hosts in the Run/Submit result.
- The topology generation script will have a `ODC_TOPO_GEN_CMD` environment variable set, containing the command line used to execute the script.

## 0.76.0 (2022-11-19)

- Cleanup output of topology generation failure: remove cmd (duplicate), shorten stdout, focus stderr.
- Allow defining expendable tasks, failure of which will be ignored and excluded from the aggregated state report.
  To define an expendable task, add a custom DDS requirement in the topology file, whose name starts with `odc_expendable_` and value `true`, e.g.:
  ```xml
  <declrequirement name="odc_expendable_task" type="custom" value="true" />
  ```
  And use the requirement in the task declaration:
  ```xml
  <decltask name="Processor">
    <exe>odc-ex-processor</exe>
    <requirements>
      <name>odc_expendable_task</name>
    </requirements>
  </decltask>
  ```

## 0.75.0 (2022-10-10)

- Adjust timeout on subsequent async ops within one request to avoid delaying execution beyond original timeout value.
- Include run number into the exiting task log entry, when it is valid (primarily during Running state).

## 0.74.0 (2022-07-19)

- epnc plugin removed.
- restore file is now updated after session shutdown, to avoid inconsistent entries.
- bugfix: crash during submit recovery when no agents were launched.
- defaults removed from --cmds, --cf batch options.
- defaults removed from --topo, --script, --content, --prop, --plugin, --resources options.
- defaults removed from --res of the odc-rp-epn-slurm plugin.
- improved resource validation in the Slurm plugin. More errors are caught with improved error messages.
- disallow repeated Run requests per partition. Only single Run request is allowed, until partition is shut down.
- allow skipping 'n' in the resource field, and pick up core scheduling settings from the topology.

## 0.73.2 (2022-06-30)

- State change errors are now logged as fatal.
- fixed: wrong severity logged in some cases.
- fixed: repeated task failures are counted multiple times despite being ignored.
- fixed: Use of moved-from value during Submit.

## 0.73.1 (2022-06-22)

- fixed: --restore-dir working incorrectly with missing trailing slash.
- fixed: odc-epn-topo: calib was missing prependExe.

## 0.73.0 (2022-06-11)

- Require DDS 3.7.15.
- Add support for core-based scheduling. Includes some **breaking changes** in odc-epn-topo:
    - `--recogroup` & `--calibgroup` arguments are removed. Agent groups are now set dynamically and internally by the tool.
    - Instead of the above, `--recozone` & `--calibzone` must be set, whose values must correspond to the zones used at ODC server start in the resource plugin's `--zones`.
    - To request core-based scheduling for calib topos, add `:<ncores>` in the arguments, e.g. `--calib calib_mft.xml:20`. This is actually non-breaking - omitting `:<ncores>` will simply not add any core-based scheduling.
- Add `--restore-dir` parameter to control where restore files are located.
- Add session history file. By default it is in `$HOME/.ODC/history`, can be changed via `--history-dir`.

## 0.72.0 (2022-05-27)

- Honor nMin on SetProperties command.
- Split the log output of the topology generation script provided via gRPC request on new lines.
- SetProperties command reply includes correct topology state, instead of `Undefined`.
- Include DDS session ID on Shutdown command reply.

## 0.71.1 (2022-05-20)

- Bugfix: Fixed incorrect tracking of nMin parameter after submission failures.
- Bugfix: Fixed invalid transitions being treated as task failures leading to task termination on nMin handling.

## 0.71 (2022-05-20)

- Fail earlier for devices that fail between state transitions.
- Fix tasks incorrectly being labeled as failed during nMin handling.
- Honor nMin during activation.
- Add 'ignored' task status to log & grpc reply.
- Reduce log verbosity during activation.
- Split longer log entries into multiple.
- Add host/node info to `GetState --detailed` gRPC response and log.
- Allow grpc::GetState to be processed asynchronously.
- Remove `<requiredSlots>` from resource definitions. All slots are now required.
- (impl) cmds: Disable successfull TransitionStatus (unused).
- (impl) cmds: Remove StateChangeExitingReceived command - use OnTaskDone instead.
- (impl) cmds: Replace CurrentState with StateChange.
- Fix several failing tests.
- Fix broken SetProperties parsing in Cli controllers.
- Cleanup session data on Shutdown command.
- execute(): fix race that can lead to incomplete output.
- execute(): fill stdout/stderr also on timeout.

## 0.70.1 (2022-05-07)

- Fix more missing includes in InfoLogger module

## 0.70 (2022-05-06)

- Fix missing includes in the InfoLogger module
- Require gRPC 1.1 (eaa122f)
- Require Protobuf 3.15 (eaa122f)
- Require CMake 3.21 (eaa122f) to fix https://github.com/alisw/alidist/pull/4164

## 0.69 (2022-05-01)

- Require DDS 3.7.12
- **Breaking change**: due to changes in DDS, slurm configuration has to be split in two files - Slurm config and environment config. Former is only for `#SBATCH` parameters, latter is for any kind of other environment setup or per-host calls. Example server cmd line: `odc-grpc-server--host "127.0.0.1:7777" --rp "slurm:/home/user/ODC/install/bin/odc-rp-epn-slurm --zones online:2:/home/user/slurm-online.cfg:/home/user/slurm-env.cfg calib:2:/home/user/slurm-calib.cfg:/home/user/slurm-env.cfg"`.
- Partition ID is now passed via DDS as the Slurm `--job-name` parameter.
- nMin parameter is honored during `.Submit` command.

## 0.68 (2022-04-22)

- Fix compilation error when readline is not available.

## 0.67 (2022-04-21)

- Require DDS 3.7.10.
- Always log a list of DDS agent details on submit.
- Increase controller heartbeat interval from 0.6 seconds to 10 minutes.
- Do not change gRPC verbosity depending on configured severity, or in any other way.
- Reduce log verbosity of the Status command.
- Log failed tasks/collections line by line.
- Honor nMin parameter during device state transitions.
- odc-grpc-client: log only to stdout, not to the file.
- Use timeout value of the corresponding request.

## 0.66 (2022-03-18)

- Require DDS 3.7.5
- Fail earlier if devices crash or transition to Error state.
- Reduce verbosity of GetState responses.
- Add stdout to failed topology generation error.
- Add .help command to CLI.
- odc-epn-topo: add support for --nmin parameter. Currently still unused in ODC controller, usage to be added in the next release.
- odc-grpc-server: make System, Facility and Role InfoLogger fields customizable via --infologger-system, --infologger-facility and --infologger-role cmd args.

## 0.65 (2022-02-23)

- odc-grpc-client: Add missing Run request parameters for plugin and resource.
- Promote topology generation script errors to fatal and output them line by line.

## 0.64 (2022-02-19)

- Update odc-epn-topo to handle agent groups.
- Annotate incoming gRPC request logs with client info.

## 0.63 (2022-02-12)

- Add odc-rp-epn-slurm plugin that translates AliECS resource descriptions into DDS Slurm RMS plugin configurations.
- Serialize only running sessions to the restore file.
- Require DDS 3.7.1 (for agent group names).

## 0.62 (2021-11-12)

- Log task and collection state stats.
- Increase log rotation size to 100MB.

## 0.60 (2021-10-26)

- Tune log severity levels.
- Add `--sshopt` option for `odc-rp-epn`.

## 0.58 (2021-10-22)

- Require DDS `3.5.21`.
- Log messages of state change and set property requests contain additional informastion about the device's runtime including hostname and wrk directory.
- Caching of additional information of tasks given by DDS in activate request.

## 0.56 (2021-10-18)

- Modified: improve list of failed devices and log messages for `SetProperties`.
- Modified: improve log messages of state change requests.

## 0.54 (2021-10-17)

- Added: `odc-epn-topo` learned new option `recown` allowing to specify worker node requirement for reco collection.
- Modified: `odc-reco-topo` add error monitoring task to calibration collection.
- Modified: Log gRPC replies with error severity in case of a failure.
- Modified: set `System` field of InfoLogger to `ODC`.

## 0.52 (2021-10-13)

- Added: session restore. Managed sessions are serialized to a file, specified by ID. On restart ODC tryes to attach to the running sessions. If failed then shutdown trigger is called.
- Added: optionally filter running DDS sessions in `Status` request.
- Modified: log crash of the task as `fatal` instead of `error`.
- Modified: use channel severity Boost log instead of severity Boost log.
- Added: whenever possible set `partition ID` for the log message.
- Added: set `Partition` and `Run` fields of the `InfoLogger`.
- Modified: require `InfoLogger` `2.2.0`.
- Added: optional `runnr` field to each request. If specified than run number will appear in logs.
- Added:: optional `timeout` field to each request. If specified than timeout value from request is used. Otherwise, the default global timeout is used.
- Added: `odc-epn-topo` learned a new option `--mon` allowing to optionally include error monitoring tool.

## 0.50 (2021-10-01)

- Modified: use sync version of `SetProperties` instead of async one.
- Modified: use sync version of `ChangeState` instead of async one.

## 0.46 (2021-09-29)

- Added: docs on how to run on the EPN cluster.

## 0.44 (2021-09-23)

- Added: print task path on task done event from DDS.
- Modified: bump DDS to 3.5.18.

## 0.42 (2021-09-13)

- odc-rp-epn: creation of DDS SSH config file.

## 0.40 (2021-09-07)

- Fixed: fix bug which prevents proper parsing of `.prop` request when using `odc-grpc-client`.

## 0.38 (2021-09-01)

- Added: new `content` and `script` parameters to `Activate`, `Update` and `Run` requests. One can set either a topology filepath, content or shell commands. If `content` is set than ODC creates a temp topology file with that content. If `script` is set than ODC executes the script and saves stdout to a temp topology file.
- Added: `odc-rp-epn` plugin supports array of resources.

## 0.36 (2021-08-13)

- Added: new `cmake` option `BUILD_EPN_PLUGIN` which switch on/of building of EPN resource plugin.
- Modified: `cmake` option `BUILD_PLUGINS` renamed to `BUILD_DEFAULT_PLUGINS`.
- Added: `odc-epn-topo` an EPN topology merging tool.

## 0.34 (2021-07-27)

- Fixed: Prevent parallel execution of multiple requests per partition. Resolves GH-24.

## 0.32 (2021-07-22)

- Fixed: registration of resource plugins

## 0.30 (2021-07-21)

- Modified: `fairmq::sdk::Topology` migrated to ODC.
- Modified: FairMQ `DDS` plugin migrated to ODC and renamed to `ODC`.
- Modified: DDS FairMQ examples migrated to ODC.
- Added: Request triggers. Request trigger is an external executable which can be registered and started whenever a particular request is processed.  Add `--rt` option for `odc-cli-server` and `odc-grpc-server` allowing to register request triggers.
- Added: New EPN resource plugin `odc-rp-epn` which gets a list of nodes via gRPC from `epnc` service and creates SSH config file.
- Modified: Resource plugin can be registered as a command line, not only a path.
- Fixed: Fix deadlocks in `Topology` dtor.
- Modified: Require DDS 3.5.16.

## 0.28 (2021-07-01)

- Added: optional `InfoLogger` support.
- Added:  Subscribe to DDS TaskDone events.
- Added: Improve device state change logging.
- Modified: require DDS 3.5.14.

## 0.26 (2021-06-16)

- Fixed: crash of `Activate` and `Submit` requests for empty session.
- Fixed: `Shutdown` request in case session was stopped by `dds-session` or `dds-commander` was killed.
- Added: More functional tests.
- Modified: require DDS 3.5.13.
- Added: Status request which returns a list of partition/session statuses, i.e. DDS session status, aggregated topology state.
- gRPC: async server implementation. Async server allows better control of threads. Only a single request is processed at a time. Multiple connections to the server are allowed. Async is a default for `odc-grpc-server`. Use `--sync` option to set sync mode.

## 0.24 (2021-05-17)

- Fixed: Linux compilation error.
- Added: `odc-topo` adds a single instance requirement for DPL collection.
- Added: initial version of `alfa-ci` integration.

## 0.22 (2021-05-05)

- Added: batch execution of requests from a configuration file. Filepath to a configuration file can be specified via `--cf` option added to `odc-grpc-client` and `odc-cli-server`.
- Added: batch execution for interactive mode. The set of options is the same as for executable. Either `--cmds` containg an array of requests or `--cf` containig a filepath to commands configuration file.
- Added: `.sleep` command allowing to sleep for some time between the requests. This is usefull for testing and batch execution.
- Added: optional `readline` support with command completion and searchable history.

## 0.20 (2021-04-12)

- Added: request/response logs for `odc-grpc-server`.
- Fixed: setting a timeout for odc-cli-server.

## 0.18 (2021-03-10)

- Added: [Resource plugins](docs/rp.md).
- Examples: Update topology creation example.

## 0.16 (2021-02-19)

- gRPC: set severity for gRPC library. Setting severity to dbg, inf, err also sets corresponding value of GRPC_VERBOSITY. For command line use `--severity` option of `odc-grpc-server` and `odc-grpc-client`. Resolves GH-14.

## 0.14 (2021-02-10)

- Added: new CMake build options: `BUILD_GRPC_CLIENT`, `BUILD_GRPC_SERVER`, `BUILD_CLI_SERVER`, `BUILD_EXAMPLES`. In order to build without `Protobuf` and `gRPC` dependencies one has to explicitly disable building of `odc-grpc-server` and `odc-grpc-client` via `cmake` command line options `-DBUILD_GRPC_CLIENT=OFF` and `-DBUILD_GRPC_SERVER=OFF`.
- Added: `--version` argument for all executables.

## 0.12 (2020-11-17)

- Modified: new C++ standard requirement - C++17.
- Added: new Commnad Line Interface of  `odc-cli-server` and `odc-grpc-client`. New interface was adapted for the multi-partition case: each request containes now a list of command line options.

## 0.10 (2020-10-21)

- Added: support of multiple partitions. In DDS terminology partition translates to a DDS session. ODC internally manages a mapping between a partition ID and corresponding DDS session.

### ODC gRPC protocol

- Modified: protocol was adapted to support multiple partitions (DDS sessions). Each request to ODC and each reply from ODC containes a `string partitionid` which uniquely identifies a partition. `runid` is removed.

## 0.8 (2020-08-10)

- Modified: improved error propagation. Use proper error codes and messages.
- Fixed: reset topology on shutdown. Topology can be activated and stoped multiple times.
- Added: execution of a sequence of commands in a batch mode. New "--batch" and "--cmds" command line arguments.
- Fixed: command line options of set properties request.

## 0.6 (2020-07-16)

- Added: new Run request which combines Initialize, Submit and Activate. Run request always creates a new DDS session.
- Added: new GetState request which returns a current aggregated state of FairMQ devices.
- Modified: bump DDS version is 3.5.1

### ODC gRPC protocol

- Modified: documentation of the proto file.
- Modified: StateChangeRequest changed to StateRequest.
- Modified: StateChangeReply changed to StateReply. New `state` field containing aggregated device state was added.
- Modified: implement SetPropertiesRequest instead of SetPropertyRequest. Multiple properties can be set with a single request.

## 0.4 (2020-07-01)

- Added: possibility to attach to the running DDS session.
- Added: DDS session ID in each reply.
- Added: set request timeout via command line interface.
- Added: unit tests.
- Modified: minimum required DDS version is 3.4

## 0.2 (2020-05-13)

The first stable internal release.
