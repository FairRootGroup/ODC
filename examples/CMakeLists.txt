################################################################################
# Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
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

odcConfigExampleTopo(0 ex-epn.xml                                        ex-epn.xml)
odcConfigExampleTopo(0 ex-epn-2.xml                                      ex-epn-2.xml)
odcConfigExampleTopo(4 ex-topo-groupname-crashing-expendable-task.xml.in ex-topo-groupname-crashing-expendable-task.xml)
odcConfigExampleTopo(4 ex-topo-groupname-crashing-no-calib.xml.in        ex-topo-groupname-crashing-no-calib.xml)
odcConfigExampleTopo(4 ex-topo-groupname-crashing.xml.in                 ex-topo-groupname-crashing.xml)
odcConfigExampleTopo(2 ex-topo-groupname-crashing-small.xml.in           ex-topo-groupname-crashing-small.xml)
odcConfigExampleTopo(4 ex-topo-groupname-ncores.xml.in                   ex-topo-groupname-ncores.xml)
odcConfigExampleTopo(4 ex-topo-groupname.xml.in                          ex-topo-groupname.xml)
odcConfigExampleTopo(2 ex-topo-submit-failure-test.xml.in                ex-topo-submit-failure-test.xml)
odcConfigExampleTopo(1 ex-topo-infinite.xml.in                           ex-topo-infinite.xml)
odcConfigExampleTopo(3 ex-topo-infinite.xml.in                           ex-topo-infinite-up.xml)
odcConfigExampleTopo(2 ex-topo-infinite.xml.in                           ex-topo-infinite-down.xml)

configure_file(ex-env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-env.sh)
configure_file(ex-slurm-calib.cfg ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-slurm-calib.cfg COPYONLY)
configure_file(ex-slurm-online.cfg ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-slurm-online.cfg COPYONLY)

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
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-dpl-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-dd-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/odc-rp-example.sh DESTINATION ${PROJECT_INSTALL_BINDIR})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/odc-ex-env.sh DESTINATION ${PROJECT_INSTALL_BINDIR})
