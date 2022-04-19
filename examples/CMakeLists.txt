################################################################################
# Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

# odc-epn-topo executable
add_executable(odc-epn-topo
  "odc-epn-topo.cpp"
)
target_link_libraries(odc-epn-topo
  Boost::boost
  Boost::program_options
  DDS::dds_topology_lib
)
target_include_directories(odc-epn-topo PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
if(BOOST_VERSION VERSION_LESS 1.76)
  target_compile_definitions(odc-epn-topo PRIVATE BOOST_BIND_GLOBAL_PLACEHOLDERS)
endif()

# odc-topo executable
add_executable(odc-topo
  "odc-topo.cpp"
)
target_link_libraries(odc-topo
  Boost::boost
  Boost::program_options
  DDS::dds_topology_lib
  ODC::odc
)
target_include_directories(odc-topo PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

# Configure and install ODC test topologies
macro(odcConfigExampleTopo N FILE_IN FILE_OUT)
  set(ODC_VAR_EXAMPLE_N ${N})
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${FILE_IN} ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUT} @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUT} DESTINATION ${PROJECT_INSTALL_DATADIR})
endmacro()

odcConfigExampleTopo(4 ex-dds-topology-groupname-req-crashing.xml.in ex-dds-topology-groupname-req-crashing.xml)
odcConfigExampleTopo(4 ex-dds-topology-groupname-req.xml.in ex-dds-topology-groupname-req.xml)
odcConfigExampleTopo(1 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite.xml)
odcConfigExampleTopo(3 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite-up.xml)
odcConfigExampleTopo(2 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite-down.xml)

configure_file(ex-env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-env.sh)

# Configure and install commands configuration example
set(ODC_DATADIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ex-cmds.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/ex-cmds.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ex-cmds.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})

add_executable(odc-ex-sampler sampler.cpp)
target_link_libraries(odc-ex-sampler PRIVATE FairMQ::FairMQ)

add_executable(odc-ex-processor processor.cpp)
target_link_libraries(odc-ex-processor PRIVATE FairMQ::FairMQ)

add_executable(odc-ex-sink sink.cpp)
target_link_libraries(odc-ex-sink PRIVATE FairMQ::FairMQ)

install(
  TARGETS
  odc-ex-sampler
  odc-ex-processor
  odc-ex-sink

  RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR}
)

# Install example executables
install(TARGETS odc-epn-topo EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
install(TARGETS odc-topo EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})

# Install example topologies
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-qc-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-dpl-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-dd-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/odc-rp-example.sh DESTINATION ${PROJECT_INSTALL_BINDIR})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-env.sh DESTINATION ${PROJECT_INSTALL_BINDIR})
