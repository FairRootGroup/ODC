# DDS-control

## Introduction
DDS-control project is an example of how to control/communicate with a system backed by [DDS](http://dds.gsi.de) and [FairMQ](https://github.com/FairRootGroup/FairMQ).

The project containes two main executables:
  * The DDS-control server: `dds-control-server`
  * The sample client: `sample-client`

Communication between server and client is done via [gRPC](https://grpc.io/). The interface of the DDS-control server is described in the [ddscontrol.proto](proto/ddscontrol.proto) file.

## 3-rd party dependencies

  * [Boost](https://www.boost.org/)
  * [DDS](http://dds.gsi.de)
  * [Protobuf](https://developers.google.com/protocol-buffers/)
  * [gRPC](https://grpc.io/)
  * [FairMQ](https://github.com/FairRootGroup/FairMQ)
  * [FairLogger](https://github.com/FairRootGroup/FairLogger)

## Installation form source

```bash
git clone https://github.com/FairRootGroup/DDS-control
cd DDS-control
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=[INSTALL_DIR] ..
make install
```

If dependencies are not installed in standard system directories, you can hint the installation location via `-DCMAKE_PREFIX_PATH=...` or per dependency via `-D{DEPENDENCY}_ROOT=...`. `{DEPENDENCY}` can be `BOOST`, `DDS`, `Protobuf`, `gRPC`, `FairMQ`, `FairLogger` (`*_ROOT` variables can also be environment variables).

## Usage
Start the server in foreground:
```bash
export PATH=[INSTALL_DIR]/bin:$PATH
dds-control-server
```

Start the sample client in a different terminal:
```bash
export PATH=[INSTALL_DIR]/bin:$PATH
sample-client
```

Alternatively, start the server as a background daemon (in your user session):

Linux:
```bash
# After installation, execute once
systemctl --user daemon-reload
# Then control dds-control-server via
systemctl --user start/stop/status dds-control
# View server logs
journalctl --user-unit dds-control [-f]
```

MacOS:
```bash
# TODO Someone on a mac verify this or correct, and find out where logs end up
# See https://www.launchd.info/. Also, I guess there are GUIs on Mac to do this too?
launchctl load/unload ~/Library/LaunchAgents/de.gsi.dds-control.plist
launchctl start/stop de.gsi.dds-control
```

Find more details on the usage of the `systemctl`/`launchctl` commands in the manpages
of your system.
