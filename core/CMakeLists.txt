# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc_core_lib
add_library(odc_core_lib
    "src/ControlService.h"
    "src/ControlService.cpp"
    "src/TimeMeasure.h"
    "src/CliHelper.h"
    "src/CliHelper.cpp"
    "src/CliServiceHelper.h"
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
