# Copyright 2014-2021 GSI, Inc. All rights reserved.
#
#
project(odc-tests)

include(ODCUtils)

set(TEST_ENV)
if(APPLE)
   list(APPEND TEST_ENV "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:$ENV{DYLD_LIBRARY_PATH}")
else()
  list(APPEND TEST_ENV "LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:$ENV{LD_LIBRARY_PATH}")
endif()
list(APPEND TEST_ENV "PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR}:$ENV{PATH}")


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
install(FILES odc_fairmq_lib-tests-topo.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
odc_add_boost_tests(SUITE odc_fairmq_lib
  TESTS
  async_op/cancel
  async_op/complete
  async_op/construction_with_handler
  async_op/default_construction
  async_op/timeout
  async_op/timeout2
  # multiple_topologies/change_state_full_lifecycle_concurrent # unstable
  multiple_topologies/change_state_full_lifecycle_interleaved
  multiple_topologies/change_state_full_lifecycle_serial
  topology/aggregated_topology_state_comparison
  topology/async_change_state
  topology/async_change_state_collection_view
  topology/async_change_state_concurrent
  topology/async_change_state_future
  topology/async_change_state_timeout
  topology/async_change_state_with_executor
  topology/async_set_properties_concurrent
  topology/async_set_properties_timeout
  topology/change_state
  topology/change_state_full_device_lifecycle
  topology/change_state_full_device_lifecycle2
  topology/construction
  topology/construction2
  topology/device_crashed
  topology/get_properties
  topology/mixed_state
  topology/set_and_get_properties
  topology/set_properties
  topology/set_properties_mixed
  topology/underlying_session_terminated
  topology/wait_for_state_full_device_lifecycle

  DEPS odc_core_lib

  EXTRA_ARGS -- --topo-file ${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}/odc_fairmq_lib-tests-topo.xml
  PROPERTIES TIMEOUT 10 ENVIRONMENT "${TEST_ENV}"
)
set_tests_properties(odc_fairmq_lib::topology/device_crashed PROPERTIES TIMEOUT 45)
set_tests_properties(odc_fairmq_lib::topology/underlying_session_terminated PROPERTIES TIMEOUT 45)
odc_add_boost_tests(SUITE odc_custom_commands_lib
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
