# DDS-control

## 3rd-party dependency

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