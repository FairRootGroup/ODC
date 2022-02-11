################################################################################
# Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

if(BUILD_CLI_SERVER)
  set(exe odc-cli-server)
  add_executable(${exe} "cli-server.cpp")
  target_link_libraries(${exe} PRIVATE Boost::boost Boost::program_options ODC::odc)
  install(TARGETS ${exe} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
endif()

if(TARGET ODC::grpc)
  set(exe odc-grpc-server)
  add_executable(${exe} "grpc-server.cpp")
  target_link_libraries(${exe} PRIVATE Boost::boost Boost::program_options ODC::grpc)
  install(TARGETS ${exe} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})

  set(exe odc-grpc-client)
  add_executable(${exe} "grpc-client.cpp")
  target_link_libraries(${exe} PRIVATE Boost::boost Boost::program_options ODC::grpc)
  install(TARGETS ${exe} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR})
endif()
