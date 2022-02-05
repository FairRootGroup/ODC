# Copyright 2019 GSI, Inc. All rights reserved.
#
#

include_guard()

find_package(Git)

# Sets PROJECT_VERSION
# The command produce a version like "X.Y.Z.gHASH"
# The hash suffix is "-g" + 7-char abbreviation for the tip commit of parent.
# The "g" prefix stands for "git" and is used to allow describing the version of a software depending on the SCM the software is managed with.
function(odc_get_version)

  if(GIT_FOUND AND EXISTS ${CMAKE_SOURCE_DIR}/.git)
    execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir "${CMAKE_SOURCE_DIR}/.git" describe --match "[0-9]*\\.[0-9]*" --abbrev=7 HEAD
                    COMMAND sed -e "s/-/./g"
                    OUTPUT_VARIABLE PROJECT_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()

  set(PROJECT_VERSION ${PROJECT_VERSION} PARENT_SCOPE)

endfunction()



# Generate and install CMake package
macro(odc_install_cmake_package)
  include(CMakePackageConfigHelpers)
  set(PACKAGE_INSTALL_DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
  )
  install(EXPORT ${PROJECT_NAME}Targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${PACKAGE_INSTALL_DESTINATION}
    EXPORT_LINK_INTERFACE_LIBRARIES
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
  )
  configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${PACKAGE_INSTALL_DESTINATION}
    PATH_VARS CMAKE_INSTALL_PREFIX
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${PACKAGE_INSTALL_DESTINATION}
  )
endmacro()


# odc_add_boost_tests(SUITE <suite>
#                     [TESTS <test1> [<test2> ...]]
#                     [SOURCES <source1> [<source2> ...]]
#                     [DEPS <dep1> [<dep2> ...]])
#                     [PROPERTIES <prop1> [<prop2> ...]])
#                     [EXTRA_ARGS <arg1> [<arg2> ...]]
#
# Declares new CTests based on Boost.UTF executables.
#
function(odc_add_boost_tests)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "SUITE" "TESTS;SOURCES;DEPS;PROPERTIES;EXTRA_ARGS")

    if(NOT ARG_SUITE)
        message(AUTHOR_WARNING "SUITE arg is requied. Skipping.")
        return()
    endif()

    if(NOT ARG_SOURCES)
        # guess cpp file name
        list(APPEND ARG_SOURCES ${ARG_SUITE}-tests.cpp)
    endif()

    if(NOT ARG_DEPS)
        # guess dependencies
        list(APPEND ARG_DEPS ${ARG_SUITE})
    endif()
    # always add Boost.UTF
    list(APPEND ARG_DEPS Boost::unit_test_framework)

    set(suite_target "${ARG_SUITE}-tests")

    add_executable(${suite_target} ${ARG_SOURCES})
    target_link_libraries(${suite_target} PRIVATE ${ARG_DEPS})
    install(TARGETS ${suite_target} EXPORT ${PROJECT_NAME}Targets RUNTIME DESTINATION ${PROJECT_INSTALL_TESTS})

    if(NOT ARG_TESTS)
        # declare the whole suite as a single ctest by default
        add_test(NAME ${ARG_SUITE}
                 COMMAND $<TARGET_FILE:${suite_target}> --log_level=message ${ARG_EXTRA_ARGS}
                 COMMAND_EXPAND_LISTS)
        if(ARG_PROPERTIES)
            set_tests_properties(${ARG_SUITE} PROPERTIES ${ARG_PROPERTIES})
        endif()
    else()
        foreach(test IN LISTS ARG_TESTS)
            set(name "${ARG_SUITE}::${test}")
            add_test(NAME ${name}
                     COMMAND $<TARGET_FILE:${suite_target}> --log_level=message --run_test=${test} ${ARG_EXTRA_ARGS}
                     COMMAND_EXPAND_LISTS)
            if(ARG_PROPERTIES)
                set_tests_properties(${name} PROPERTIES ${ARG_PROPERTIES})
            endif()
        endforeach()
    endif()
endfunction()
