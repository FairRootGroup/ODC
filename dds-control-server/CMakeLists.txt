# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# dds-control executable
configure_file(src/main.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp @ONLY)
set(dc_server_srcs
    "${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp"
    "src/DDSControlServer.h"
    "src/DDSControlServer.cpp"
    "src/DDSControlService.h"
    "src/DDSControlService.cpp"
)
set(dc_server_exec dds-control-server)
add_executable(${dc_server_exec} ${dc_server_srcs} ${dc_proto_srcs} ${dc_grpc_srcs})
target_link_libraries(${dc_server_exec} "${GRPC_GRPC++_LIBRARY}" "${GRPC_LIBRARY}" protobuf::libprotobuf DDS::dds_topology_lib DDS::dds_intercom_lib DDS::dds_tools_lib Boost::boost Boost::program_options FairMQ::SDK)
target_include_directories(${dc_server_exec} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "${GRPC_INCLUDE_DIR}"
)
