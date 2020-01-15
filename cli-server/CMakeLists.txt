# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc-cli-server executable
configure_file(src/main.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp @ONLY)
add_executable(odc-cli-server
    "${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp"
    "src/CliControlService.h"
    "src/CliControlService.cpp"
)
target_link_libraries(odc-cli-server
  Boost::boost
  Boost::program_options
  odc_core_lib
)
target_include_directories(odc-cli-server PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
