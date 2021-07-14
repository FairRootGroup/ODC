# Copyright 2019 GSI, Inc. All rights reserved.
#
#

add_library(odc_fairmq_lib OBJECT
    "src/AsioAsyncOp.h"
    "src/AsioBase.h"
    "src/Topology.h"
    "src/Semaphore.h"
    "src/Semaphore.cpp"
    "src/Traits.h"
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
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
install(TARGETS odc_fairmq_lib EXPORT ${PROJECT_NAME}Targets)
