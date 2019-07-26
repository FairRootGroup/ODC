# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# Proto file
set(dc_proto "ddscontrol.proto")

# Generated sources
protobuf_generate_cpp(dc_proto_srcs dc_proto_hdrs ${CMAKE_CURRENT_BINARY_DIR} ${dc_proto})
grpc_generate_cpp(dc_grpc_srcs dc_grpc_hdrs ${CMAKE_CURRENT_BINARY_DIR} ${dc_proto})

add_library(dc-grpc STATIC
  ${dc_proto_srcs}
  ${dc_proto_hdrs}
  ${dc_grpc_srcs}
  ${dc_grpc_hdrs}
)
target_include_directories(dc-grpc PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
