# Copyright 2019 GSI, Inc. All rights reserved.
#
#

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
