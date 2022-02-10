################################################################################
# Copyright (C) 2012-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

# the name prefix `FairMQPlugin_` is required by the FairMQ plugin mechanism
set(plugin FairMQPlugin_odc)
add_library(${plugin} SHARED ODC.cpp ODC.h)
add_library(ODC::${plugin} ALIAS ${plugin})
target_compile_features(${plugin} PUBLIC cxx_std_17)
target_link_libraries(${plugin} PRIVATE
  Boost::boost
  DDS::dds_intercom_lib
  DDS::dds_protocol_lib
  FairMQ::FairMQ
  odc_custom_commands_lib
)
target_include_directories(${plugin} PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
set_target_properties(${plugin} PROPERTIES CXX_VISIBILITY_PRESET hidden)
# set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fairmq)
if(BOOST_VERSION VERSION_LESS 1.76)
  target_compile_definitions(${plugin} PRIVATE BOOST_BIND_GLOBAL_PLACEHOLDERS)
endif()
install(TARGETS ${plugin}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${PROJECT_INSTALL_LIBDIR}
)
