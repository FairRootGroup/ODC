# Copyright 2019 GSI, Inc. All rights reserved.
#
#
find_path(readline_INCLUDE_DIR NAMES readline/readline.h)
find_library(readline_LIBRARY NAMES readline)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(readline REQUIRED_VARS readline_INCLUDE_DIR readline_LIBRARY)

if(readline_FOUND AND NOT TARGET readline::readline)
  add_library(readline::readline SHARED IMPORTED)
  set_target_properties(readline::readline PROPERTIES
    IMPORTED_LOCATION "${readline_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${readline_INCLUDE_DIR}")
endif()
