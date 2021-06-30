# Online Device Control

## Introduction
The Online Device Control project control/communicate with a graph (topology) of [FairMQ](https://github.com/FairRootGroup/FairMQ) devices using [DDS](http://dds.gsi.de) or PMIx (under development)

The project containes a library and several executables:
  * The core library `odc-core-lib`.
  * The gRPC server `odc-grpc-server` is a sample implementation of the server based on the `odc-core-lib`.
  * The gRPC client `odc-grpc-client` is a sample implementation of client.
  * The CLI server `odc-cli-server` is another sample implementation of the server which doesn't require gRPC installation.

Communication between `odc-grpc-server` and `odc-grpc-client` is done via [gRPC](https://grpc.io/). The interface of the `odc-grpc-server` is described in the [odc.proto](grpc-proto/odc.proto) file.

## Command Mapping

| Command | ODC reaction |
| ----------- | ------------ |
| Initialize | Create a new DDS session or attach to an existing DDS session |
| Submit | Submit DDS agents (deploys a dynamic cluster) according to a specified computing resources. Can be called multiple times in order to submit more DDS agents (allocate more resources). |
| Activate | Activate DDS topology (devices enter `Idle` state) |
| Run | Combine Initialize, Submit and Activate commands. A new DDS session is always created. |
| Update |  Updates a topology (up or down scale number of tasks or any other topology change). It consists of 3 commands: `Reset`, `Activate` and `Configure`. Can be called multiple times. |
| Configure | Transition devices into `Ready` state (via `InitDevice` -> `CompleteInit` -> `Bind` -> `Connect` -> `InitTask` transitions) |
| SetProperties | Change devices configuration |
| GetState | Get current aggregated state of devices |
| Start | Transition devices into `Running` state (via `Run` transition) |
| Stop | Transition devices into `Ready` state (via `Stop` transition) |
| Reset | Transition devices into `Idle` state (via `ResetTask` -> `ResetDevice` transitions) |
| Terminate | Shut devices down via `End` transition |
| Shutdown | Shutdown DDS session |
| Status | Show statuses of managed partitions/sessions |


## 3-rd party dependencies

  * [Boost](https://www.boost.org/)
  * [DDS](http://dds.gsi.de)
  * [Protobuf](https://developers.google.com/protocol-buffers/)
  * [gRPC](https://grpc.io/)
  * [FairMQ](https://github.com/FairRootGroup/FairMQ)
  * [FairLogger](https://github.com/FairRootGroup/FairLogger)
  * [InfoLogger](https://github.com/AliceO2Group/InfoLogger/) (optional)

For macOS we recommend to install gRPC via `brew` which also installs `Protobuf` and other dependencies:
```bash
brew install grpc
```

## Installation form source

```bash
git clone https://github.com/FairRootGroup/ODC
cd ODC && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=[INSTALL_DIR] ..
make install
```

If dependencies are not installed in standard system directories, you can hint the installation location via `-DCMAKE_PREFIX_PATH=...` or per dependency via `-D{DEPENDENCY}_ROOT=...`. `{DEPENDENCY}` can be `BOOST`, `DDS`, `Protobuf`, `gRPC`, `FairMQ`, `FairLogger` (`*_ROOT` variables can also be environment variables).

In order to build without `Protobuf` and `gRPC` dependencies one has to explicitly disable building of `odc-grpc-server` and `odc-grpc-client` via `cmake` command line options `-DBUILD_GRPC_CLIENT=OFF` and `-DBUILD_GRPC_SERVER=OFF`.

### CMake command line options

  * `-DBUILD_TESTS=OFF` disables building of tests.
  * `-DBUILD_GRPC_CLIENT=OFF` disables building of gRPC client.
  * `-DBUILD_GRPC_SERVER=OFF` disables building of gRPC server.
  * `-DBUILD_CLI_SERVER=OFF` disables building of CLI server.
  * `-DBUILD_EXAMPLES=OFF` disables building of examples.
  * `-DBUILD_PLUGINS=OFF` disables building of plugins.
  * `-DBUILD_INFOLOGGER=ON` enables `InfoLogger` support.

## Installation with aliBuild

Alternatively, ODC and 3-rd party dependencies can be installed using [aliBuild](https://github.com/alisw/alibuild):

```bash
mkdir INSTALL_DIR && cd INSTALL_DIR
git clone https://github.com/alisw/alidist.git
aliBuild --default o2-dataflow build ODC
```

## Basic usage
Start `odc-grpc-server` in foreground:
```bash
export PATH=[INSTALL_DIR]/bin:$PATH && odc-grpc-server
```

Start `odc-grpc-client` in a different terminal:
```bash
export PATH=[INSTALL_DIR]/bin:$PATH && odc-grpc-client
```

Alternatively, if gRPC is not installed, start `odc-cli-server` in foreground:
```bash
export PATH=[INSTALL_DIR]/bin:$PATH && odc-cli-server
```

By default this example uses [localhost plugin](http://dds.gsi.de/doc/nightly/RMS-plugins.html#localhost-plugin) of [DDS](https://github.com/FairRootGroup/DDS) and topologies installed in `INSTALL_DIR/share/odc`.

The basic sequence of commands for interactive mode:
```
.init
.submit
.activate
.config
.start
.stop
.reset
.term
.down
.quit
```
The full list of available commands is printed on start of `odc-grpc-client` and `odc-cli-server`. Most of the commands have additional options. Use `--help` to print help message with a list of available options, i.e. `.start --help`. 

A sequence of request can also be executed in a batch mode using `--batch` option of `odc-grpc-client` and `odc-cli-server`. The sequence of commands can be changed via `--cmds` or `--cf` options. `--cmds` accepts an array of commands together with command line options seperated by space. For example, initializing and shutting down two partitions (sessions) in batch mode:
```bash
odc-grpc-client --batch --cmds ".init --id a1b2" ".init --id c3d4" ".down --id a1b2" ".down --id c3d4"
```
`--cf` accepts a filepath to the confiuration file with a list of commands, i.e.:
```bash
odc-grpc-client --batch --cf /path/to/my/file.cfg
```
Each line in a configuration file represents a single command. An example can be found [here](examples/cmds.cfg.in).

`.batch` command is also available in interactive mode allowing to execute a common set of command pipelines with less typing. The command also accepts either `--cmds` or `--cf` options.

## Daemon

Alternatively, start the ODC server as a background daemon (in your user session):

Linux:
```bash
# After installation, execute once
systemctl --user daemon-reload
# Then control odc-grpc-server via
systemctl --user start/stop/status odc
# View server logs
journalctl --user-unit odc [-f]
```
One can also use `systemd-run` to start the ODC server:
```bash
systemd-run --user --unit=odc odc-grpc-server
```

MacOS:
```bash
# TODO Someone on a mac verify this or correct, and find out where logs end up
# See https://www.launchd.info/. Also, I guess there are GUIs on Mac to do this too?
launchctl load/unload ~/Library/LaunchAgents/de.gsi.odc.plist
launchctl start/stop de.gsi.odc-grpc-server
```

Find more details on the usage of the `systemctl`/`systemd-run`/`launchctl` commands in the manpages
of your system.

## Documentation

1. [Frequently Asked Questions](docs/faq.md)
2. [Examples](examples)
3. [Release notes](ReleaseNotes.md)
4. [Resource plugins](docs/rp.md)
