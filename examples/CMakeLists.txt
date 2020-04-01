# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# Configure and install ODC test topologies
macro(odcConfigExampleTopo N FILE_IN FILE_OUT)
    set(ODC_VAR_EXAMPLE_N ${N})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${FILE_IN} ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUT} @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUT} DESTINATION ${PROJECT_INSTALL_DATADIR})
endmacro()
    
odcConfigExampleTopo(10 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite.xml)
odcConfigExampleTopo(20 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite-up.xml)
odcConfigExampleTopo(5 ex-dds-topology-infinite.xml.in ex-dds-topology-infinite-down.xml)

# Install QC example topology
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ex-qc-topology.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
