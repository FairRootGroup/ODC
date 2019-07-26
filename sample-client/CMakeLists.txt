# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# sample-client executable
configure_file(src/main.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp @ONLY)
set(dc_client_srcs
    "${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp"
    "src/DDSControlClient.h"
    "src/DDSControlClient.cpp"
)
set(dc_client_exec sample-client)
add_executable(${dc_client_exec} ${dc_client_srcs} ${dc_proto_srcs} ${dc_grpc_srcs})
target_link_directories(${dc_client_exec} PUBLIC "${Boost_LIBRARY_DIRS}")
target_link_libraries(${dc_client_exec} "${GRPC_GRPC++_LIBRARY}" "${GRPC_LIBRARY}" protobuf::libprotobuf Boost::boost Boost::program_options)
target_include_directories(${dc_client_exec} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "${GRPC_INCLUDE_DIR}"
)
