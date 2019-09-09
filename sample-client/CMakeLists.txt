# Copyright 2019 GSI, Inc. All rights reserved.
#
#

# sample-client executable
configure_file(src/main.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp @ONLY)
add_executable(sample-client
    "${CMAKE_CURRENT_BINARY_DIR}/src/main.cpp"
    "src/DDSControlClient.h"
    "src/DDSControlClient.cpp"
)
target_link_libraries(sample-client
    Boost::boost
    Boost::program_options
    dc-grpc
)
target_include_directories(sample-client PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "${GRPC_INCLUDE_DIR}"
)
