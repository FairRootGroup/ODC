# Copyright 2019 GSI, Inc. All rights reserved.
#
#
project(ODC)

cmake_minimum_required( VERSION 3.12.0 FATAL_ERROR )
cmake_policy(VERSION 3.12...3.14)

# Define CMAKE_INSTALL_*DIR variables
include(GNUInstallDirs)

string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

set(BUILD_SHARED_LIBS ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

# Cmake find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find Protobuf installation
find_package(Protobuf)
message(STATUS "Using protobuf ${Protobuf_VERSION}")

# Find gRPC installation
find_package(gRPC)
message(STATUS "Using gRPC ${GRPC_VERSION}")

# Find DDS installation
find_package(DDS 3.0 CONFIG REQUIRED)
message(STATUS "Using DDS ${DDS_VERSION}")

# Find Boost installation
set(Boost_Components log log_setup thread program_options filesystem system regex)
find_package(Boost 1.67 REQUIRED COMPONENTS  ${Boost_Components})

# Find FairMQ
find_package(FairMQ 1.4.6 REQUIRED COMPONENTS sdk)
message(STATUS "Using FairMQ ${FairMQ_GIT_VERSION}")
if(FairMQ_CXX_STANDARD VERSION_GREATER CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${FairMQ_CXX_STANDARD})
endif()

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
set (CMAKE_INSTALL_PREFIX "$ENV{HOME}/ODC" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Source Code Formatting using clang-format
add_custom_target(format-code
       COMMAND ${CMAKE_SOURCE_DIR}/utils/update_format.sh ${CMAKE_SOURCE_DIR}
       COMMENT "Updating source code formatting.")

# Define install dirs
set(PROJECT_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(PROJECT_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(PROJECT_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME_LOWER})
set(PROJECT_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER})

# Targets
add_subdirectory(core)
if(gRPC_FOUND)
   add_subdirectory(grpc-proto)
   add_subdirectory(grpc-server)
   add_subdirectory(grpc-client)
endif()
add_subdirectory(cli-server)
add_subdirectory(examples)

#
# Install
#
if(gRPC_FOUND)
   install(TARGETS odc-grpc-server odc-grpc-client RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
endif()
install(TARGETS odc_core_lib LIBRARY DESTINATION ${PROJECT_INSTALL_LIBDIR})
install(TARGETS odc-cli-server RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})

# Daemon config
if(APPLE)
    configure_file(utils/launchd/de.gsi.odc.plist.in de.gsi.odc.plist @ONLY)
    set(daemon_file "${CMAKE_BINARY_DIR}/de.gsi.odc.plist")
    set(daemon_destination "$ENV{HOME}/Library/LaunchAgents")
else()
    configure_file(utils/systemd/odc.service.in odc.service @ONLY)
    set(daemon_file "${CMAKE_BINARY_DIR}/odc.service")
    if(NOT ENV{XDG_CONFIG_HOME})
      set(config_home "$ENV{HOME}/.config")
    else()
      set(config_home "$ENV{XDG_CONFIG_HOME}")
    endif()
    set(daemon_destination "${config_home}/systemd/user")
endif()

install(FILES ${daemon_file} DESTINATION ${daemon_destination})
