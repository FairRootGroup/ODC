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

set(ODC_DATADIR "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_DATADIR}")
string(RANDOM LENGTH 8 TEST_SESSION1)
string(RANDOM LENGTH 8 TEST_SESSION2)

set(target odc-cli-server)

# Test odc-cli-server with a certain set of options
configure_file(cmd_set_1.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_1.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_1.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(test ${target}::cmd_set_1)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --severity dbg --batch --cf ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_1.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "Status code: ERROR" ENVIRONMENT "${TEST_ENV}")

string(RANDOM LENGTH 8 TEST_SESSION)

# Test odc-cli-server with the same set of options, except init+submit+activate are replaced with run command
configure_file(cmd_set_1_run.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_1_run.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_1_run.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(test ${target}::cmd_set_1_run)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --severity dbg --batch --cf ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_1_run.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "Status code: ERROR" ENVIRONMENT "${TEST_ENV}")

string(RANDOM LENGTH 8 TEST_SESSION)

# Test odc-cli-server by sending different commands without running session
configure_file(cmd_set_2.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_2.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_2.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(test ${target}::cmd_set_2)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --severity dbg --batch --cf ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_2.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "Status code: ERROR" ENVIRONMENT "${TEST_ENV}")

string(RANDOM LENGTH 8 TEST_SESSION)

# Test --script option of odc-cli-server
configure_file(cmd_set_3_with_script.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_3_with_script.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_3_with_script.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(test ${target}::cmd_set_3_with_script)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --severity dbg --batch --cf ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_3_with_script.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "Status code: ERROR" ENVIRONMENT "${TEST_ENV}")

string(RANDOM LENGTH 8 TEST_SESSION)

# Test --extract-topo-resources option of odc-cli-server
configure_file(cmd_set_4_extract.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_4_extract.cfg @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_4_extract.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
set(test ${target}::cmd_set_4_extract)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --severity dbg --batch --cf ${CMAKE_CURRENT_BINARY_DIR}/test_cmd_set_4_extract.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "Status code: ERROR" ENVIRONMENT "${TEST_ENV}")

# Test options from the provided example
set(test ${target}::cmd_set_example)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --severity dbg --batch --cf ${CMAKE_BINARY_DIR}/examples/ex-cmds.cfg)
set_tests_properties(${test} PROPERTIES TIMEOUT 60 ENVIRONMENT "${TEST_ENV}")

# Test options from the provided example as --cmds
file(STRINGS ${CMAKE_BINARY_DIR}/examples/ex-cmds.cfg cmds)
set(test ${target}::cmd_set_example_as_cmds)
add_test(NAME ${test} COMMAND $<TARGET_FILE:odc-cli-server> --severity dbg --batch --cmds ${cmds})
set_tests_properties(${test} PROPERTIES TIMEOUT 60 ENVIRONMENT "${TEST_ENV}")

# Test nmin feature that reduces set of active devices when some fail, without failing entire topology
macro(add_nmin_test TITLE PASS FAIL)
  set(TEST_SETUP test_${TITLE})
  string(RANDOM LENGTH 8 TEST_SESSION)
  configure_file(topos/${TITLE}.xml.in ${CMAKE_CURRENT_BINARY_DIR}/${TEST_SETUP}.xml @ONLY)
  configure_file(nmin.cfg.in           ${CMAKE_CURRENT_BINARY_DIR}/${TEST_SETUP}.cfg @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TEST_SETUP}.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TEST_SETUP}.cfg DESTINATION ${PROJECT_INSTALL_DATADIR})
  add_test(NAME ${TITLE} COMMAND $<TARGET_FILE:odc-cli-server> --timeout 20 --severity dbg --batch --cf ${CMAKE_CURRENT_BINARY_DIR}/${TEST_SETUP}.cfg)
  set_tests_properties(${TITLE} PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "${PASS}" FAIL_REGULAR_EXPRESSION "${FAIL}" ENVIRONMENT "${TEST_ENV}")
endmacro()

# nmin is 0 - is the same as when it is not defined.
add_nmin_test(nmin_0 "Status code: ERROR" "")

# nmin: a collection outside of group should fail
add_nmin_test(nmin_col_outside_group "Status code: ERROR" "")

# nmin equals n - failure of one collection should lead a failure of entire topology
add_nmin_test(nmin_eq_n "Status code: ERROR" "")

# nmin greater n - invalid combination, any failure leads to complete topology failure
add_nmin_test(nmin_gt_n "Status code: ERROR" "")

# nmin is less than n - recovery should be possible if conditions satisfied
add_nmin_test(nmin_lt_n "" "Status code: ERROR")

# nmin is less than n - recovery should be possible if conditions satisfied and failing processes are hanging
add_nmin_test(nmin_lt_n_hanging "" "Status code: ERROR")

# TODO: fix this case
# multiple collections per group are currently not allowed (might change in the future) - failure results in complete topology failure
# add_nmin_test(nmin_multiple_cols_per_group "Status code: ERROR" "")

# when nmin is not defined, failure of one task leads to entire topology failure
add_nmin_test(nmin_not_defined "Status code: ERROR" "")

# currently failure of tasks outside of collection lead to entire topology fail
add_nmin_test(nmin_tasks_outside_col "Status code: ERROR" "")

# currently failure of tasks outside of group leads to entire topology fail
add_nmin_test(nmin_tasks_outside_group "Status code: ERROR" "")

# Boost.UTF tests
install(FILES topos/odc-tests-topo.xml DESTINATION ${PROJECT_INSTALL_DATADIR})
odc_add_boost_tests(SUITE odc
  TESTS
  async_op/cancel
  async_op/complete
  async_op/construction_with_handler
  async_op/default_construction
  async_op/timeout
  async_op/timeout2
#   multiple_topologies/change_state_full_lifecycle_concurrent
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
  PROPERTIES TIMEOUT 60 ENVIRONMENT "${TEST_ENV}"
)

odc_add_boost_tests(SUITE cc
  TESTS
  format/construction
  format/serialization_binary
  format/serialization_json

  DEPS ODC::cc

  PROPERTIES TIMEOUT 60 ENVIRONMENT "${TEST_ENV}"
)

odc_add_boost_tests(SUITE parameters
  TESTS
  creation/odc_rp_same_simple
  creation/odc_rp_same_zones
  creation/odc_rp_epn_slurm_zones
  creation/odc_rp_epn_slurm_zones_group_without_n_with_tasks
  creation/odc_rp_epn_slurm_zones_group_without_n_without_tasks
  creation/odc_rp_epn_slurm_ncores
  creation/odc_rp_epn_slurm_ncores_without_n
  creation/odc_extract_epn

  DEPS ODC::odc

  PROPERTIES TIMEOUT 60 ENVIRONMENT "${TEST_ENV}"
)

odc_add_boost_tests(SUITE requirements
  TESTS
  extraction/simple
  extraction/zones_from_agent_groupnames
  extraction/zones_with_ncores
  extraction/nmin
  extraction/epn
  extraction/epn_2

  DEPS ODC::odc

  PROPERTIES TIMEOUT 60 ENVIRONMENT "${TEST_ENV}"
)
