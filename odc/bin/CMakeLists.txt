# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc-cli-server executable
add_executable(odc-cli-server
  "main.cpp"
  "CliControlService.h"
  "CliControlService.cpp"
)
target_link_libraries(odc-cli-server
  Boost::boost
  Boost::program_options
  odc_core_lib
)
target_include_directories(odc-cli-server PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
install(TARGETS odc-cli-server EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
