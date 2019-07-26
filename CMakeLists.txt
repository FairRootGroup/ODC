# Copyright 2019 GSI, Inc. All rights reserved.
#
#
project(DDS-control)

cmake_minimum_required( VERSION 3.12.0 FATAL_ERROR )
cmake_policy(VERSION 3.12...3.14)

set(BUILD_SHARED_LIBS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Cmake find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find Protobuf installation
find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

# Find gRPC installation
find_package(gRPC REQUIRED)
message(STATUS "Using gRPC ${GRPC_VERSION}")

# Find DDS installation
find_package(DDS CONFIG REQUIRED)
message(STATUS "Using DDS ${DDS_VERSION}")

# Find Boost installation
set(Boost_Components log log_setup thread program_options filesystem system regex)
find_package(Boost 1.67 REQUIRED COMPONENTS  ${Boost_Components})

# Find FairMQ
find_package(FairMQ 1.4.6 REQUIRED COMPONENTS sdk)
message(STATUS "Using FairMQ ${FairMQ_GIT_VERSION}")

# Find FairLogger
find_package(FairLogger REQUIRED)
message(STATUS "Using FairLogger ${FairLogger_VERSION}")

# https://cmake.org/Wiki/CMake_RPATH_handling
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-Wl,--enable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-Wl,--enable-new-dtags")
  endif()
endif()

# Install directory
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
set (CMAKE_INSTALL_PREFIX "$ENV{HOME}/DDS-control" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Source Code Formatting using clang-format ??? DONT write to source directory...
# add_custom_target(format-code
       # COMMAND ${CMAKE_SOURCE_DIR}/utils/update_format.sh ${CMAKE_SOURCE_DIR}
       # COMMENT "Updating source code formatting.")

# Targets
add_subdirectory(proto)
add_subdirectory(dds-control-server)
add_subdirectory(sample-client)

#
# Install
#
install(TARGETS dds-control-server sample-client RUNTIME DESTINATION bin)
