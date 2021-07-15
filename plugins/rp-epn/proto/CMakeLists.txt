# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# Proto file
set(epnc_proto_file "epnc.proto")

# Generated sources
protobuf_generate_cpp(epns_proto_srcs epnc_proto_hdrs ${epnc_proto_file})
grpc_generate_cpp(epnc_grpc_srcs epnc_grpc_hdrs ${CMAKE_CURRENT_BINARY_DIR} ${epnc_proto_file})

add_library(epnc_grpc_proto_lib STATIC
  ${epns_proto_srcs}
  ${epnc_proto_hdrs}
  ${epnc_grpc_srcs}
  ${epnc_grpc_hdrs}
)
target_include_directories(epnc_grpc_proto_lib PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
target_link_libraries(epnc_grpc_proto_lib PUBLIC
  protobuf::libprotobuf
  gRPC::grpc++
)
