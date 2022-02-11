################################################################################
# Copyright (C) 2020-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

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

set(target odc-rp-epn)
add_executable(${target}
  odc-rp-epn.cpp
  EpncClient.h
)
target_link_libraries(${target}
  Boost::boost
  Boost::program_options
  Boost::thread
  DDS::dds_misc_lib
  ODC::odc
  epnc_grpc_proto_lib
)
target_include_directories(${target} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "${GRPC_INCLUDE_DIR}"
)
if(BOOST_VERSION VERSION_LESS 1.76)
  target_compile_definitions(${target} PRIVATE BOOST_BIND_GLOBAL_PLACEHOLDERS)
endif()

install(TARGETS ${target} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
