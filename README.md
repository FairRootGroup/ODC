# DDS-control

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
Start the server:
```bash
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
