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