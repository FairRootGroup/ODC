# Copyright 2019 GSI, Inc. All rights reserved.
#
#

add_library(odc_fairmq_lib STATIC
  "AsioAsyncOp.h"
  "AsioBase.h"
  "Topology.h"
  "Semaphore.h"
  "Semaphore.cpp"
  "Traits.h"
)
target_link_libraries(odc_core_lib PUBLIC
  DDS::dds_topology_lib
  DDS::dds_intercom_lib
  DDS::dds_tools_lib
  Boost::boost
  odc_custom_commands_lib
  FairMQ::FairMQ
)
target_include_directories(odc_fairmq_lib INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
