################################################################################
# Copyright (C) 2019-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

set(target odc_custom_commands_lib)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/CustomCommandsFormat.h
  COMMAND $<TARGET_FILE:flatbuffers::flatc> -c -o ${CMAKE_CURRENT_BINARY_DIR}/src
          src/CustomCommandsFormat.fbs
  COMMAND ${CMAKE_COMMAND} -E rename
          ${CMAKE_CURRENT_BINARY_DIR}/src/CustomCommandsFormat_generated.h
          ${CMAKE_CURRENT_BINARY_DIR}/src/CustomCommandsFormat.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# JSON serialization needs to see the .fbs file at run time, save it as
# constexpr string instead of locating/opening it every time
file(STRINGS src/CustomCommandsFormat.fbs tmp)
list(JOIN tmp "\n" commands_format_def_fbs)
configure_file(src/CustomCommandsFormatDef.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/src/CustomCommandsFormatDef.h
)

add_library(${target} STATIC
  src/CustomCommands.cpp
  src/CustomCommands.h
  ${CMAKE_CURRENT_BINARY_DIR}/src/CustomCommandsFormat.h
  ${CMAKE_CURRENT_BINARY_DIR}/src/CustomCommandsFormatDef.h
)

set(_flatbuffers flatbuffers::flatbuffers_shared)
if(NOT TARGET flatbuffers::flatbuffers_shared AND TARGET flatbuffers::flatbuffers)
  set(_flatbuffers flatbuffers::flatbuffers)
endif()

target_link_libraries(${target} PUBLIC FairMQ::FairMQ ${_flatbuffers})
target_compile_features(${target} PUBLIC cxx_std_17)
target_include_directories(${target} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
