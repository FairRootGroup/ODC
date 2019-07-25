# Copyright 2019 GSI, Inc. All rights reserved.
#
#
project(DDS-control)

cmake_minimum_required( VERSION 3.11.0 FATAL_ERROR )

set(BUILD_SHARED_LIBS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Cmake find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find Protobuf installation
find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

# Find gRPC installation
find_package(gRPC REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

# Find DDS installation
find_package(DDS CONFIG REQUIRED)
message(STATUS "Using DDS ${DDS_VERSION}")

# Find Boost installation
set(Boost_Components log log_setup thread program_options filesystem system regex)
find_package(Boost 1.67 REQUIRED COMPONENTS  ${Boost_Components})
#find_package(Boost 1.67 REQUIRED)

# Find FairMQ
find_package(FairMQ REQUIRED COMPONENTS sdk)
message(STATUS "Using FairMQ ${FairMQ_VERSION}" ${FairMQ_GIT_VERSION})

# Find FairLogger
find_package(FairLogger REQUIRED)
message(STATUS "Using FairLogger ${FairLogger_VERSION}")

# Install directory
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
set (CMAKE_INSTALL_PREFIX "$ENV{HOME}/DDS-control" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Source Code Formatting using clang-format
add_custom_target(format-code
       COMMAND ${CMAKE_SOURCE_DIR}/utils/update_format.sh ${CMAKE_SOURCE_DIR}
       COMMENT "Updating source code formatting.")

# Proto file
get_filename_component(dc_proto "${CMAKE_SOURCE_DIR}/proto/ddscontrol.proto" ABSOLUTE)
get_filename_component(dc_proto_path "${dc_proto}" PATH)

# Generated sources
set(dc_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/ddscontrol.pb.cc")
set(dc_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/ddscontrol.pb.h")
set(dc_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/ddscontrol.grpc.pb.cc")
set(dc_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/ddscontrol.grpc.pb.h")

add_custom_command(
    OUTPUT "${dc_proto_srcs}" "${dc_proto_hdrs}" "${dc_grpc_srcs}" "${dc_grpc_hdrs}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${dc_proto_path}"
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         "${dc_proto}"
    DEPENDS "${dc_proto}"
    COMMENT "Generating gRPC and Protobuf C++ files."
    VERBATIM
)

# Include generated *.pb.h files
#include_directories("${CMAKE_CURRENT_BINARY_DIR}" "${Protobuf_INCLUDE_DIRS}" "${GRPC_INCLUDE_DIR}" "${DDS_INCDIR}")

# dds-control executable
set(dc_server_srcs 
    "dds-control-server/src/main.cpp" 
    "dds-control-server/src/DDSControlServer.h" 
    "dds-control-server/src/DDSControlServer.cpp"
    "dds-control-server/src/DDSControlService.h" 
    "dds-control-server/src/DDSControlService.cpp"
)
set(dc_server_exec dds-control-server)
add_executable(${dc_server_exec} ${dc_server_srcs} ${dc_proto_srcs} ${dc_grpc_srcs})
target_link_libraries(${dc_server_exec} "${GRPC_GRPC++_LIBRARY}" "${GRPC_LIBRARY}" "${Protobuf_LIBRARIES}" DDS::dds_topology_lib DDS::dds_intercom_lib DDS::dds_tools_lib Boost::boost Boost::program_options FairMQ::SDK)
target_include_directories(${dc_server_exec} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${Protobuf_INCLUDE_DIRS}" "${GRPC_INCLUDE_DIR}")

# sample-client executable
set(dc_client_srcs 
    "sample-client/src/main.cpp"
    "sample-client/src/DDSControlClient.h"
    "sample-client/src/DDSControlClient.cpp"
)
set(dc_client_exec sample-client)
add_executable(${dc_client_exec} ${dc_client_srcs} ${dc_proto_srcs} ${dc_grpc_srcs})
target_link_directories(${dc_client_exec} PUBLIC "${Boost_LIBRARY_DIRS}")
target_link_libraries(${dc_client_exec} "${GRPC_GRPC++_LIBRARY}" "${GRPC_LIBRARY}" ${Protobuf_LIBRARIES} Boost::program_options)
target_include_directories(${dc_client_exec} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${Protobuf_INCLUDE_DIRS}" "${GRPC_INCLUDE_DIR}")

#add_subdirectory(dds-control-server)
#add_subdirectory(sample-client)

#
# Install
#
install(TARGETS "${dc_server_exec}" RUNTIME DESTINATION bin)
install(TARGETS "${dc_client_exec}" RUNTIME DESTINATION bin)

