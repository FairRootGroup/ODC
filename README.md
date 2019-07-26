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
Before starting the server we need to source DDS_env.sh script
```bash
cd [DDS_INSTALL_DIR]
source DDS_env.sh
```

Start the server:
```bash
cd [DDS-control_INSTALL_DIR]
./dds-control-server --host "localhost:50051"
```

Start the sample client in a different terminal:
```bash
./sample-client --host "localhost:50051" --topo [FAIRMQ_INSTALL_DIR]/share/fairmq/ex-dds-topology.xml 
```

## 3rd-party installation

### gRPC

#### install go
~~~~~~~~~~~~~~~~~
curl -O https://storage.googleapis.com/golang/go1.12.6.darwin-amd64.tar.gz
tar -xzvf go1.12.6.darwin-amd64.tar.gz
export PATH=$PATH:<ROOTFOLDER>/go/bin
~~~~~~~~~~~~~~~~~
#### install gRPC

~~~~~~~~~~~~~~~~~
git clone -b $(curl -L https://grpc.io/release) https://github.com/grpc/grpc
cd grpc
git submodule update --init

mkdir cmake_build
cd cmake_build/
cmake -DCMAKE_INSTALL_PREFIX:PATH=/Users/anar/Development/gRPC-install
make -j install
~~~~~~~~~~~~~~~~~
