# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc-grpc-server executable
add_executable(odc-grpc-server
    "src/main.cpp"
    "src/GrpcControlServer.h"
    "src/GrpcControlServer.cpp"
    "src/GrpcControlService.h"
    "src/GrpcControlService.cpp"
)
target_link_libraries(odc-grpc-server
  Boost::boost
  Boost::program_options
  odc_core_lib
  odc_grpc_proto_lib
)
target_include_directories(odc-grpc-server PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "${GRPC_INCLUDE_DIR}"
)
