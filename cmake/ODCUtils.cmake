# Copyright 2019 GSI, Inc. All rights reserved.
#
#

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
