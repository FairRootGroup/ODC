# Copyright 2019 GSI, Inc. All rights reserved.
#
#

add_subdirectory(proto)

set(target odc-rp-epn)
add_executable(${target}
  src/odc-rp-epn.cpp
  src/EpncClient.h
  src/EpncClient.cpp
)
target_link_libraries(${target}
  Boost::boost
  Boost::program_options
  DDS::dds_misc_lib
  odc_core_lib
  epnc_grpc_proto_lib
)
target_include_directories(${target} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "${GRPC_INCLUDE_DIR}"
)

install(TARGETS ${target} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
