# Copyright 2019 GSI, Inc. All rights reserved.
#
#
PROJECT(DDS-control)

cmake_minimum_required( VERSION 3.11.0 FATAL_ERROR )

ADD_SUBDIRECTORY(proto)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

#ADD_EXECUTABLE(main main.cpp)
#TARGET_LINK_LIBRARIES(main proto ${PROTOBUF_LIBRARY})