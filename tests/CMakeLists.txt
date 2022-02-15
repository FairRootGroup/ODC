# Copyright 2014-2021 GSI, Inc. All rights reserved.
#
#
project(odc-tests)

include(ODCUtils)

set(TEST_ENV)
if(APPLE)
   list(APPEND TEST_ENV "DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:${Boost_LIBRARY_DIRS}:$ENV{DYLD_LIBRARY_PATH}")
else()
  list(APPEND TEST_ENV "LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}:${Boost_LIBRARY_DIRS}:$ENV{LD_LIBRARY_PATH}")
endif()
list(APPEND TEST_ENV "PATH=${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR}:$ENV{PATH}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tmp")
list(APPEND TEST_ENV "TMPDIR=${CMAKE_BINARY_DIR}/tmp")

# Test odc-cli-server with default set of options
set(target odc-cli-server)
set(test ${target}::default)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --batch)
set_tests_properties(${test} PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "Status code: ERROR" ENVIRONMENT "${TEST_ENV}")

# Test odc-cli-server by sending different commands without running session
install(FILES batch_cmds.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(target odc-cli-server)
set(test ${target}::batch_cmds_from_file)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --batch --cf ${CMAKE_CURRENT_SOURCE_DIR}/batch_cmds.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 10 PASS_REGULAR_EXPRESSION "Sleeping 100 ms" ENVIRONMENT "${TEST_ENV}")

# Test `--script` option of odc-cli-server
set(ODC_DATADIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}")
configure_file(batch_cmds_with_script.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/batch_cmds_with_script.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/batch_cmds_with_script.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(target odc-cli-server)
set(test ${target}::batch_cmds_from_file_with_script)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --batch --cf ${CMAKE_CURRENT_BINARY_DIR}/batch_cmds_with_script.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 10 FAIL_REGULAR_EXPRESSION "Status code: ERROR" ENVIRONMENT "${TEST_ENV}")

#
# Boost.UTF tests
#
install(FILES odc-tests-topo.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
odc_add_boost_tests(SUITE odc
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

  DEPS ODC::odc

  EXTRA_ARGS -- --topo-file ${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}/odc-tests-topo.xml
  PROPERTIES TIMEOUT 10 ENVIRONMENT "${TEST_ENV}"
)
set_tests_properties(odc::topology/device_crashed PROPERTIES TIMEOUT 45)
set_tests_properties(odc::topology/underlying_session_terminated PROPERTIES TIMEOUT 45)
odc_add_boost_tests(SUITE cc
  TESTS
  format/construction
  format/serialization_binary
  format/serialization_json

  DEPS ODC::cc

  PROPERTIES TIMEOUT 10 ENVIRONMENT "${TEST_ENV}"
)
