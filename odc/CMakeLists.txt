################################################################################
# Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

add_subdirectory(cc)

set(ODC_DATADIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}")
set(ODC_BINDIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR}")
configure_file(BuildConstants.h.in ${CMAKE_CURRENT_BINARY_DIR}/BuildConstants.h @ONLY)
configure_file(Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/Version.h @ONLY)

set(target "odc")
add_library(${target} STATIC
  "${CMAKE_CURRENT_BINARY_DIR}/BuildConstants.h"
  "${CMAKE_CURRENT_BINARY_DIR}/Version.h"
  "AsioAsyncOp.h"
  "AsioBase.h"
  "CliControlService.cpp"
  "CliControlService.h"
  "CliHelper.cpp"
  "CliHelper.h"
  "CliServiceHelper.h"
  "CmdsFile.h"
  "Controller.cpp"
  "Controller.h"
  "DDSSubmit.h"
  "Error.cpp"
  "Error.h"
  "InfoLogger.h"
  "Logger.h"
  "LoggerSeverity.h"
  "MiscUtils.h"
  "PluginManager.h"
  "Process.h"
  "Restore.h"
  "Semaphore.h"
  "Stats.cpp"
  "Stats.h"
  "TimeMeasure.h"
  "Topology.h"
  "Traits.h"
)
target_link_libraries(${target} PUBLIC
  DDS::dds_topology_lib
  DDS::dds_intercom_lib
  DDS::dds_tools_lib
  Boost::boost
  Boost::filesystem
  Boost::log_setup
  Boost::log
  FairMQ::FairMQ
  ODC::cc
)
add_library("${PROJECT_NAME}::${target}" ALIAS "${target}")
target_include_directories(${target} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
)
target_compile_definitions(${target} PUBLIC BOOST_LOG_DYN_LINK)
if(BOOST_VERSION VERSION_LESS 1.76)
  target_compile_definitions(${target} PUBLIC BOOST_BIND_GLOBAL_PLACEHOLDERS)
endif()
if(readline_FOUND)
  target_compile_definitions(${target} PUBLIC READLINE_AVAIL)
  target_link_libraries(${target} PUBLIC readline::readline)
endif()
if (InfoLogger_FOUND)
  target_compile_definitions(${target} PUBLIC INFOLOGGER_AVAIL)
  target_link_libraries(${target} PUBLIC AliceO2::InfoLogger)
endif()

if(BUILD_GRPC_CLIENT OR BUILD_GRPC_SERVER)
   add_subdirectory(grpc)
endif()
add_subdirectory(bin)
