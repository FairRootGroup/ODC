# Copyright 2014-2021 GSI, Inc. All rights reserved.
#
#
project(odc-tests)

include(ODCUtils)

set(TEST_ENV)
if(APPLE)
   list(APPEND TEST_ENV "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:$DYLD_LIBRARY_PATH")
else()
   list(APPEND TEST_ENV "LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:$LD_LIBRARY_PATH")
endif()
list(APPEND TEST_ENV "PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR}:$PATH")


#
# Test odc-cli-server with default set of options
#
set(target odc-cli-server)
set(test ${target}::default)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --batch)
set_tests_properties(${test} PROPERTIES
    TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "Status code: ERROR"
    ENVIRONMENT "${TEST_ENV}"
)

#
# Test odc-cli-server by sending different commands without running session
#
install(FILES cmds_test_1.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(target odc-cli-server)
set(test ${target}::test_1)
add_test(NAME ${test}
         COMMAND $<TARGET_FILE:odc-cli-server> --batch --cf ${CMAKE_CURRENT_SOURCE_DIR}/cmds_test_1.cfg
)
set_tests_properties(${test} PROPERTIES
    TIMEOUT 10
    PASS_REGULAR_EXPRESSION "Sleeping 100 ms"
    ENVIRONMENT "${TEST_ENV}"
)

#
# Boost.UTF tests
#
odc_add_boost_tests(SUITE odc_custom_commands
  TESTS
  format/construction
  format/serialization_binary
  format/serialization_json

  PROPERTIES TIMEOUT 10 ENVIRONMENT "${TEST_ENV}"
)

#
# Configure and install run_tests.sh
#
configure_file(run_tests.sh.in ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh DESTINATION ${PROJECT_INSTALL_TESTS})
