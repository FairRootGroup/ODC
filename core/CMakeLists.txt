# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc_core_lib
set(ODC_DATADIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}")
configure_file(src/BuildConstants.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/BuildConstants.h @ONLY)
add_library(odc_core_lib
    "${CMAKE_CURRENT_BINARY_DIR}/src/BuildConstants.h"
    "src/ControlService.h"
    "src/ControlService.cpp"
    "src/TimeMeasure.h"
    "src/CliHelper.h"
    "src/CliHelper.cpp"
    "src/CliServiceHelper.h"
    "src/Logger.h"
)
target_link_libraries(odc_core_lib
  DDS::dds_topology_lib
  DDS::dds_intercom_lib
  DDS::dds_tools_lib
  Boost::boost
  FairMQ::SDK
)
target_include_directories(odc_core_lib PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
