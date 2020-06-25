# Copyright 2014 GSI, Inc. All rights reserved.
#
#
project(odc-tests)

add_executable(odc_core_lib-tests src/odc_core_lib-tests.cpp)

target_link_libraries(
    odc_core_lib-tests
    PUBLIC
    odc_core_lib
    Boost::unit_test_framework
)

configure_file(run_tests.sh.in ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh DESTINATION ${PROJECT_INSTALL_TESTS})

install(TARGETS odc_core_lib-tests EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_TESTS})
