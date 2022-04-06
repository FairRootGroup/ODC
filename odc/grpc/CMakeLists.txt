################################################################################
# Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

set(proto_file "odc.proto")
protobuf_generate_cpp(proto_srcs proto_hdrs ${proto_file})
grpc_generate_cpp(grpc_srcs grpc_hdrs ${CMAKE_CURRENT_BINARY_DIR} ${proto_file})

set(target "grpc")
add_library(${target} STATIC
  ${proto_srcs}
  ${proto_hdrs}
  ${grpc_srcs}
  ${grpc_hdrs}
  "AsyncController.h"
  "Controller.h"
  "ControlClient.h"
  "SyncController.h"
)
add_library("${PROJECT_NAME}::${target}" ALIAS "${target}")
set_target_properties(${target} PROPERTIES OUTPUT_NAME "${PROJECT_NAME_LOWER}-${target}")
target_include_directories(${target} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>"
  "${GRPC_INCLUDE_DIR}"
)
target_link_libraries(${target} PUBLIC
  protobuf::libprotobuf
  gRPC::grpc++
  ODC::odc
)

set(exe odc-grpc-server)
add_executable(${exe} "Server.cpp")
target_link_libraries(${exe} PRIVATE Boost::boost Boost::program_options ODC::grpc)
install(TARGETS ${exe} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})

set(exe odc-grpc-client)
add_executable(${exe} "Client.cpp")
target_link_libraries(${exe} PRIVATE Boost::boost Boost::program_options ODC::grpc)
install(TARGETS ${exe} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})