################################################################################
#    Copyright (C) 2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

set(target odc-rp-epn-slurm)
add_executable(${target}
  odc-rp-epn-slurm.cpp
)
target_link_libraries(${target}
  Boost::boost
  Boost::program_options
  Boost::thread
  odc_core_lib
)
target_include_directories(${target} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
if(BOOST_VERSION VERSION_LESS 1.76)
  target_compile_definitions(${target} PRIVATE BOOST_BIND_GLOBAL_PLACEHOLDERS)
endif()

install(TARGETS ${target} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
