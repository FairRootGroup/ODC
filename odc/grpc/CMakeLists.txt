################################################################################
# Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

set(target "grpc")
add_library(${target} STATIC
  "Controller.h"
  "ControlClient.h"
  "odc.proto"
)
add_library("${PROJECT_NAME}::${target}" ALIAS "${target}")
set_target_properties(${target} PROPERTIES OUTPUT_NAME "${PROJECT_NAME_LOWER}-${target}")
target_include_directories(${target} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>"
)
target_link_libraries(${target} PUBLIC
  protobuf::libprotobuf
  gRPC::grpc++
  ODC::odc
)
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET grpc LANGUAGE cpp)
protobuf_generate(TARGET grpc LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")

set(exe odc-grpc-server)
add_executable(${exe} "Server.cpp")
target_link_libraries(${exe} PRIVATE Boost::boost Boost::program_options ODC::grpc)
install(TARGETS ${exe} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})

set(exe odc-grpc-client)
add_executable(${exe} "Client.cpp")
target_link_libraries(${exe} PRIVATE Boost::boost Boost::program_options ODC::grpc)
install(TARGETS ${exe} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
