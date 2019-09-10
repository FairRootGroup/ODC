# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# dds-control executable
configure_file(src/main.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp @ONLY)
add_executable(dds-control-server
    "${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp"
    "src/DDSControlServer.h"
    "src/DDSControlServer.cpp"
    "src/DDSControlService.h"
    "src/DDSControlService.cpp"
    "src/TimeMeasure.h"
)
target_link_libraries(dds-control-server
  DDS::dds_topology_lib
  DDS::dds_intercom_lib
  DDS::dds_tools_lib
  Boost::boost
  Boost::program_options
  FairMQ::SDK
  dc-grpc
)
target_include_directories(dds-control-server PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "${GRPC_INCLUDE_DIR}"
)
