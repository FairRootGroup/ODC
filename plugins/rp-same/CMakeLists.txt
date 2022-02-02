# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc-rp-same executable
add_executable(odc-rp-same
  "odc-rp-same.cpp"
)
target_link_libraries(odc-rp-same
  Boost::boost
  Boost::program_options
  Boost::thread
  odc_core_lib
)
target_include_directories(odc-rp-same PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

install(TARGETS odc-rp-same EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
