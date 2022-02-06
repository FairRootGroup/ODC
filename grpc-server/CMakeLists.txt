# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# odc-grpc-server executable
add_executable(odc-grpc-server
  "main.cpp"
  "GrpcService.h"
  "GrpcService.cpp"
  "GrpcSyncService.h"
  "GrpcAsyncService.h"
  "GrpcAsyncService.cpp"
)
target_link_libraries(odc-grpc-server PUBLIC
  Boost::boost
  Boost::program_options
  odc_core_lib
  odc_grpc_proto_lib
)
target_include_directories(odc-grpc-server PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "${GRPC_INCLUDE_DIR}"
)
install(TARGETS odc-grpc-server EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
