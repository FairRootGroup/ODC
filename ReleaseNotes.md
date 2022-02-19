# ODC Release Notes

## v0.64 (2022-02-19)

- Update odc-epn-topo to handle agent groups.
- Annotate incoming gRPC request logs with client info.

## v0.63 (2022-02-12)

- Add odc-rp-epn-slurm plugin that translates AliECS resource descriptions into DDS Slurm RMS plugin configurations.
- Serialize only running sessions to the restore file.
- Require DDS 3.7.1 (for agent group names).

## v0.62 (2021-11-12)
### ODC common
* Log task and collection state stats.
* Increase log rotation size to 100MB.


## v0.60 (2021-10-26)
### ODC common
* Tune log severity levels.
* Add `--sshopt` option for `odc-rp-epn`.


## v0.58 (2021-10-22)
### ODC common
* Require DDS `3.5.21`.
* Log messages of state change and set property requests contain additional informastion about the device's runtime including hostname and wrk directory.
* Caching of additional information of tasks given by DDS in activate request.


## v0.56 (2021-10-18)
### ODC common
Modified: improve list of failed devices and log messages for `SetProperties`.
Modified: improve log messages of state change requests.


## v0.54 (2021-10-17)
### ODC common
Added: `odc-epn-topo` learned new option `recown` allowing to specify worker node requirement for reco collection.
Modified: `odc-reco-topo` add error monitoring task to calibration collection.
Modified: Log gRPC replies with error severity in case of a failure.
Modified: set `System` field of InfoLogger to `ODC`.


## v0.52 (2021-10-13)
### ODC common
Added: session restore. Managed sessions are serialized to a file, specified by ID. On restart ODC tryes to attach to the running sessions. If failed then shutdown trigger is called.
Added: optionally filter running DDS sessions in `Status` request.
Modified: log crash of the task as `fatal` instead of `error`.
Modified: use channel severity Boost log instead of severity Boost log.
Added: whenever possible set `partition ID` for the log message.
Added: set `Partition` and `Run` fields of the `InfoLogger`.
Modified: require `InfoLogger` `2.2.0`.
Added: optional `runnr` field to each request. If specified than run number will appear in logs.
Added:: optional `timeout` field to each request. If specified than timeout value from request is used. Otherwise, the default global timeout is used.
Added: `odc-epn-topo` learned a new option `--mon` allowing to optionally include error monitoring tool.


## v0.50 (2021-10-01)
### ODC common
Modified: use sync version of `SetProperties` instead of async one.
Modified: use sync version of `ChangeState` instead of async one.


## v0.46 (2021-09-29)
### ODC common
Added: docs on how to run on the EPN cluster.


## v0.44 (2021-09-23)
### ODC common
Added: print task path on task done event from DDS.
Modified: bump DDS to 3.5.18.


## v0.42 (2021-09-13)
### odc-rp-epn
Fixed: creation of DDS SSH config file.


## v0.40 (2021-09-07)
### ODC common
Fixed: fix bug which prevents proper parsing of `.prop` request when using `odc-grpc-client`.


## v0.38 (2021-09-01)
### ODC common
Added: new `content` and `script` parameters to `Activate`, `Update` and `Run` requests. One can set either a topology filepath, content or shell commands. If `content` is set than ODC creates a temp topology file with that content. If `script` is set than ODC executes the script and saves stdout to a temp topology file.
Added: `odc-rp-epn` plugin supports array of resources.


## v0.36 (2021-08-13)
### ODC common
Added: new `cmake` option `BUILD_EPN_PLUGIN` which switch on/of building of EPN resource plugin.
Modified: `cmake` option `BUILD_PLUGINS` renamed to `BUILD_DEFAULT_PLUGINS`.
Added: `odc-epn-topo` an EPN topology merging tool.


## v0.34 (2021-07-27)
### ODC common
Fixed: Prevent parallel execution of multiple requests per partition. Resolves GH-24.


## v0.32 (2021-07-22)
### ODC common
Fixed: registration of resource plugins


## v0.30 (2021-07-21)
### ODC common
Modified: `fairmq::sdk::Topology` migrated to ODC.
Modified: FairMQ `DDS` plugin migrated to ODC and renamed to `ODC`.
Modified: DDS FairMQ examples migrated to ODC.
Added: Request triggers. Request trigger is an external executable which can be registered and started whenever a particular request is processed.  Add `--rt` option for `odc-cli-server` and `odc-grpc-server` allowing to register request triggers.
Added: New EPN resource plugin `odc-rp-epn` which gets a list of nodes via gRPC from `epnc` service and creates SSH config file.
Modified: Resource plugin can be registered as a command line, not only a path.
Fixed: Fix deadlocks in `Topology` dtor.
Modified: Require DDS 3.5.16.


## v0.28 (2021-07-01)
### ODC common
Added: optional `InfoLogger` support.
Added:  Subscribe to DDS TaskDone events.
Added: Improve device state change logging.
Modified: require DDS 3.5.14.


## v0.26 (2021-06-16)
### ODC common
Fixed: crash of `Activate` and `Submit` requests for empty session.
Fixed: `Shutdown` request in case session was stopped by `dds-session` or `dds-commander` was killed.
Added: More functional tests.
Modified: require DDS 3.5.13.
Added: Status request which returns a list of partition/session statuses, i.e. DDS session status, aggregated topology state.

### gRPC
Added: async server implementation. Async server allows better control of threads. Only a single request is processed at a time. Multiple connections to the server are allowed. Async is a default for `odc-grpc-server`. Use `--sync` option to set sync mode.


## v0.24 (2021-05-17)
### ODC common
Fixed: Linux compilation error.
Added: `odc-topo` adds a single instance requirement for DPL collection.
Added: initial version of `alfa-ci` integration.


## v0.22 (2021-05-05)
### ODC common
Added: batch execution of requests from a configuration file. Filepath to a configuration file can be specified via `--cf` option added to `odc-grpc-client` and `odc-cli-server`.
Added: batch execution for interactive mode. The set of options is the same as for executable. Either `--cmds` containg an array of requests or `--cf` containig a filepath to commands configuration file.
Added: `.sleep` command allowing to sleep for some time between the requests. This is usefull for testing and batch execution.
Added: optional `readline` support with command completion and searchable history.


## v0.20 (2021-04-12)
### ODC common
Added: request/response logs for `odc-grpc-server`.
Fixed: setting a timeout for odc-cli-server.


## v0.18 (2021-03-10)
### ODC common
Added: [Resource plugins](docs/rp.md).

### Examples
Modified: Update topology creation example.


## v0.16 (2021-02-19)
### gRPC
Added: set severity for gRPC library. Setting severity to dbg, inf, err also sets corresponding value of GRPC_VERBOSITY. For command line use `--severity` option of `odc-grpc-server` and `odc-grpc-client`. Resolves GH-14.


## v0.14 (2021-02-10)
### ODC common
Added: new CMake build options: `BUILD_GRPC_CLIENT`, `BUILD_GRPC_SERVER`, `BUILD_CLI_SERVER`, `BUILD_EXAMPLES`. In order to build without `Protobuf` and `gRPC` dependencies one has to explicitly disable building of `odc-grpc-server` and `odc-grpc-client` via `cmake` command line options `-DBUILD_GRPC_CLIENT=OFF` and `-DBUILD_GRPC_SERVER=OFF`.
Added: `--version` argument for all executables.


## v0.12 (2020-11-17)
### ODC common
Modified: new C++ standard requirement - C++17.
Added: new Commnad Line Interface of  `odc-cli-server` and `odc-grpc-client`. New interface was adapted for the multi-partition case: each request containes now a list of command line options.


## v0.10 (2020-10-21)
### ODC common
Added: support of multiple partitions. In DDS terminology partition translates to a DDS session. ODC internally manages a mapping between a partition ID and corresponding DDS session.

### ODC gRPC protocol
Modified: protocol was adapted to support multiple partitions (DDS sessions). Each request to ODC and each reply from ODC containes a `string partitionid` which uniquely identifies a partition. `runid` is removed.


## v0.8 (2020-08-10)

### ODC common
Modified: improved error propagation. Use proper error codes and messages.
Fixed: reset topology on shutdown. Topology can be activated and stoped multiple times.
Added: execution of a sequence of commands in a batch mode. New "--batch" and "--cmds" command line arguments.
Fixed: command line options of set properties request.


## v0.6 (2020-07-16)

### ODC common
Added: new Run request which combines Initialize, Submit and Activate. Run request always creates a new DDS session.
Added: new GetState request which returns a current aggregated state of FairMQ devices.
Modified: bump DDS version is 3.5.1

### ODC gRPC protocol
Modified: documentation of the proto file.
Modified: StateChangeRequest changed to StateRequest.
Modified: StateChangeReply changed to StateReply. New `state` field containing aggregated device state was added.
Modified: implement SetPropertiesRequest instead of SetPropertyRequest. Multiple properties can be set with a single request.


## v0.4 (2020-07-01)

### ODC common
Added: possibility to attach to the running DDS session.
Added: DDS session ID in each reply.
Added: set request timeout via command line interface.
Added: unit tests.
Modified: minimum required DDS version is 3.4


## v0.2 (2020-05-13)

The first stable internal release.
