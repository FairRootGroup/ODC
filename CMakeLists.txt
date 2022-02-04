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

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(ODCUtils)
include(CTest)

odc_get_version()

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

# Define preferred defaults for some global CMake settings
if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif()
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()
add_compile_options(-Wall -Wextra)
if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if(NOT DEFINED CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(FairCMakeModules 1.0.0 REQUIRED QUIET)
include(FairFindPackage2) # for find_package2

if(BUILD_GRPC_CLIENT OR BUILD_GRPC_SERVER)
  find_package2(PRIVATE Protobuf REQUIRED)
  find_package2(PRIVATE gRPC REQUIRED)
endif()
find_package2(PRIVATE DDS VERSION 3.6 REQUIRED)
find_package2(PRIVATE FairMQ VERSION 1.4.26 REQUIRED COMPONENTS fairmq)
set(Boost_Components log log_setup thread program_options filesystem system regex)
if(BUILD_TESTS)
  set(Boost_Components ${Boost_Components} unit_test_framework)
endif(BUILD_TESTS)
find_package2(PRIVATE Boost VERSION 1.67 REQUIRED COMPONENTS ${Boost_Components})
find_package2(PRIVATE Flatbuffers REQUIRED)
find_package2(PRIVATE readline)
if(BUILD_INFOLOGGER)
  find_package2(PRIVATE InfoLogger REQUIRED)
endif()
find_package2_implicit_dependencies() # Always call last after latest find_package2

# https://cmake.org/Wiki/CMake_RPATH_handling
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()
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

# Print project config summary
include(FairFormattedOutput) # for color codes
include(FairSummary)
message(STATUS "  ")
message(STATUS "  ${Cyan}${PROJECT_NAME} VERSION${CR}  ${BGreen}${PROJECT_VERSION}${CR}")
fair_summary_global_cxx_flags_standard()
set(Threads_PREFIX "<system>")
fair_summary_package_dependencies()
message(STATUS "  ")
message(STATUS "  ${Cyan}INSTALL PREFIX${CR}  ${BGreen}${CMAKE_INSTALL_PREFIX}${CR} (change with ${BMagenta}-DCMAKE_INSTALL_PREFIX=...${CR})")
message(STATUS "  ")
