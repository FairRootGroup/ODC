################################################################################
# Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################
cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
cmake_policy(VERSION 3.12...3.22)

project(ODC LANGUAGES CXX)

# Cmake find modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# ODC utilities
include(ODCUtils)

# CTest
include(CTest)

odc_get_version()

message(STATUS "Building ${PROJECT_NAME} ${PROJECT_VERSION} ...")

string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

option(BUILD_TESTS "Build ODC unit tests" ON)
option(BUILD_GRPC_CLIENT "Build gRPC client of ODC" ON)
option(BUILD_GRPC_SERVER "Build gRPC server of ODC" ON)
option(BUILD_CLI_SERVER "Build CLI server of ODC" ON)
option(BUILD_DEFAULT_PLUGINS "Build default plugins of ODC" ON)
option(BUILD_EPN_PLUGIN "Build EPN plugin of ODC" ON)
option(BUILD_EXAMPLES "Build ODC examples" ON)
option(BUILD_INFOLOGGER "Build with InfoLogger support" OFF)

# Define CMAKE_INSTALL_*DIR variables
include(GNUInstallDirs)

# Define install dirs
set(PROJECT_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(PROJECT_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(PROJECT_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME_LOWER})
set(PROJECT_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER})
set(PROJECT_INSTALL_TESTS tests)
set(PROJECT_INSTALL_DOCDIR ${CMAKE_INSTALL_DATADIR}/doc/${PROJECT_NAME_LOWER})

set(BUILD_SHARED_LIBS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

if(BUILD_GRPC_CLIENT OR BUILD_GRPC_SERVER)
    # Find Protobuf installation
    find_package(Protobuf REQUIRED)
    message(STATUS "Using protobuf ${Protobuf_VERSION}")

    # Find gRPC installation
    find_package(gRPC REQUIRED)
    message(STATUS "Using gRPC ${GRPC_VERSION}")
endif()

# Find DDS installation
find_package(DDS 3.5.21 CONFIG REQUIRED)
message(STATUS "Using DDS ${DDS_VERSION}")

# Find Boost installation
set(Boost_Components log log_setup thread program_options filesystem system regex)
if(BUILD_TESTS)
  set(Boost_Components ${Boost_Components} unit_test_framework)
endif(BUILD_TESTS)
find_package(Boost 1.67 REQUIRED COMPONENTS  ${Boost_Components})

# Find FairLogger
find_package(FairLogger REQUIRED)
message(STATUS "Using FairLogger ${FairLogger_VERSION}")
message(STATUS "Looking for FairLogger dependencies: ${FairLogger_PACKAGE_DEPENDENCIES}" )
foreach(dep IN LISTS FairLogger_PACKAGE_DEPENDENCIES)
  find_package(${dep} ${FairLogger_${dep}_VERSION})
endforeach()

# Find FairMQ
find_package(FairMQ 1.4.26 REQUIRED COMPONENTS fairmq)
message(STATUS "Using FairMQ ${FairMQ_GIT_VERSION}")
if(FairMQ_CXX_STANDARD VERSION_GREATER CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${FairMQ_CXX_STANDARD})
endif()
message(STATUS "Looking for FairMQ dependencies: ${FairMQ_PACKAGE_DEPENDENCIES}" )
foreach(dep IN LISTS FairMQ_PACKAGE_DEPENDENCIES)
    if(FairMQ_${dep}_COMPONENTS)
      find_package(${dep} ${FairMQ_${dep}_VERSION} COMPONENTS ${FairMQ_${dep}_COMPONENTS})
    else()
      find_package(${dep} ${FairMQ_${dep}_VERSION})
    endif()
endforeach()

# Find Flatbuffers
find_package(Flatbuffers REQUIRED)

# Find optional readline package
find_package(readline)

if(BUILD_INFOLOGGER)
    # Find InfoLogger installation
    find_package(InfoLogger REQUIRED)
    message(STATUS "Using InfoLogger ${InfoLogger_VERSION}")
endif()

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

# Targets
add_subdirectory(core)
add_subdirectory(fairmq)
if(BUILD_GRPC_CLIENT OR BUILD_GRPC_SERVER)
   add_subdirectory(grpc-proto)
endif()
if(BUILD_GRPC_SERVER)
   add_subdirectory(grpc-server)
endif()
if(BUILD_GRPC_CLIENT)
   add_subdirectory(grpc-client)
endif()
if(BUILD_CLI_SERVER)
   add_subdirectory(cli-server)
endif()
if(BUILD_DEFAULT_PLUGINS)
   add_subdirectory(plugins/rp-same)
endif()
if(BUILD_EPN_PLUGIN)
   add_subdirectory(plugins/rp-epn)
endif()
if(BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()
if(BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()

# Install
SET(ODC_DOC_FILES
    ${CMAKE_SOURCE_DIR}/LICENSE
    ${CMAKE_SOURCE_DIR}/ReleaseNotes.md
)
install(FILES ${ODC_DOC_FILES} DESTINATION ${PROJECT_INSTALL_DOCDIR})

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

odc_install_cmake_package()
