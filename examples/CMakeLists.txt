# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc-topo executable
add_executable(odc-topo
    "src/odc-topo.cpp"
)
target_link_libraries(odc-topo
  Boost::boost
  Boost::program_options
  DDS::dds_topology_lib
  odc_core_lib
)
target_include_directories(odc-topo PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)

# Configure and install ODC test topologies
macro(odcConfigExampleTopo N FILE_IN FILE_OUT)
    set(ODC_VAR_EXAMPLE_N ${N})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${FILE_IN} ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUT} @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUT} DESTINATION ${PROJECT_INSTALL_DATADIR})
endmacro()

odcConfigExampleTopo(1 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite.xml)
odcConfigExampleTopo(3 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite-up.xml)
odcConfigExampleTopo(2 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite-down.xml)

configure_file(ex-env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-env.sh)

# Configure and install commands configuration example
set(ODC_DATADIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmds.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/cmds.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cmds.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})

add_executable(odc-ex-sampler src/sampler.cpp)
target_link_libraries(odc-ex-sampler PRIVATE FairMQ::FairMQ)

add_executable(odc-ex-processor src/processor.cpp)
target_link_libraries(odc-ex-processor PRIVATE FairMQ::FairMQ)

add_executable(odc-ex-sink src/sink.cpp)
target_link_libraries(odc-ex-sink PRIVATE FairMQ::FairMQ)

install(
  TARGETS
  odc-ex-sampler
  odc-ex-processor
  odc-ex-sink

  RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR}
)

# Install example executables
install(TARGETS odc-topo EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})

# Install example topologies
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-qc-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-dpl-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-dd-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/odc-rp-example.sh DESTINATION ${PROJECT_INSTALL_BINDIR})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-env.sh DESTINATION ${PROJECT_INSTALL_BINDIR})
