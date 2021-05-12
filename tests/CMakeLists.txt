# Copyright 2014 GSI, Inc. All rights reserved.
#
#
project(odc-tests)

if(APPLE)
   set(TEST_ENV "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:$ENV{DYLD_LIBRARY_PATH}")
else()
   set(TEST_ENV "LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:$ENV{LD_LIBRARY_PATH}")
endif()

#
# Test odc-cli-server with default set of options
#
add_test(NAME odc-cli-server_default COMMAND "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR}/odc-cli-server" --batch)
set_tests_properties(odc-cli-server_default PROPERTIES
    TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "Status code: ERROR"
    ENVIRONMENT ${TEST_ENV}
)

#
# odc_core_lib tests
#
add_executable(odc_core_lib-tests src/odc_core_lib-tests.cpp)
target_link_libraries(
    odc_core_lib-tests
    PUBLIC
    odc_core_lib
    Boost::unit_test_framework
)
install(TARGETS odc_core_lib-tests EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_TESTS})
add_test(NAME odc_core_lib COMMAND "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_TESTS}/odc_core_lib-tests")
set_tests_properties(odc_core_lib PROPERTIES
    TIMEOUT 10
    ENVIRONMENT ${TEST_ENV}
)

#
# Configure and install run_tests.sh
#
configure_file(run_tests.sh.in ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh DESTINATION ${PROJECT_INSTALL_TESTS})
