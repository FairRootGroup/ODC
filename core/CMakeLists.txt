# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# BuildConstants header
set(ODC_DATADIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}")
set(ODC_BINDIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR}")
configure_file(BuildConstants.h.in ${CMAKE_CURRENT_BINARY_DIR}/BuildConstants.h @ONLY)

# Version header
configure_file(Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/Version.h @ONLY)

# odc_core_lib target
add_library(odc_core_lib STATIC
  "${CMAKE_CURRENT_BINARY_DIR}/Version.h"
  "${CMAKE_CURRENT_BINARY_DIR}/BuildConstants.h"
  "ControlService.h"
  "ControlService.cpp"
  "TimeMeasure.h"
  "CliHelper.h"
  "CliHelper.cpp"
  "CliServiceHelper.h"
  "Logger.h"
  "LoggerSeverity.h"
  "InfoLogger.h"
  "Error.h"
  "Error.cpp"
  "MiscUtils.h"
  "DDSSubmit.h"
  "Process.h"
  "CmdsFile.h"
  "CmdsFile.cpp"
  "Def.h"
  "PluginManager.h"
  "PluginManager.cpp"
  "Restore.h"
  "Restore.cpp"
  "Stats.h"
  "Stats.cpp"
)
target_link_libraries(odc_core_lib PUBLIC
  DDS::dds_topology_lib
  DDS::dds_intercom_lib
  DDS::dds_tools_lib
  Boost::boost
  Boost::filesystem
  Boost::log_setup
  Boost::log
  odc_fairmq_lib
  FairMQ::FairMQ
)
target_include_directories(odc_core_lib PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
target_compile_definitions(odc_core_lib PUBLIC BOOST_LOG_DYN_LINK)
if(BOOST_VERSION VERSION_LESS 1.76)
  target_compile_definitions(odc_core_lib PUBLIC BOOST_BIND_GLOBAL_PLACEHOLDERS)
endif()
if(readline_FOUND)
  target_compile_definitions(odc_core_lib PUBLIC READLINE_AVAIL)
  target_link_libraries(odc_core_lib PUBLIC readline::readline)
endif()
if (InfoLogger_FOUND)
  target_compile_definitions(odc_core_lib PUBLIC INFOLOGGER_AVAIL)
  target_link_libraries(odc_core_lib PUBLIC AliceO2::InfoLogger)
endif()
