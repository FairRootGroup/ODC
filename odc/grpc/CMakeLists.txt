################################################################################
# Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

# Proto file
set(odc_proto_file "odc.proto")

# Generated sources
protobuf_generate_cpp(odc_proto_srcs odc_proto_hdrs ${odc_proto_file})
grpc_generate_cpp(odc_grpc_srcs odc_grpc_hdrs ${CMAKE_CURRENT_BINARY_DIR} ${odc_proto_file})

add_library(odc_grpc_proto_lib STATIC
  ${odc_proto_srcs}
  ${odc_proto_hdrs}
  ${odc_grpc_srcs}
  ${odc_grpc_hdrs}
)
target_include_directories(odc_grpc_proto_lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
target_link_libraries(odc_grpc_proto_lib PUBLIC
  protobuf::libprotobuf
  gRPC::grpc++
)
