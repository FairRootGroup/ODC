# Copyright 2019 GSI, Inc. All rights reserved.
#
#

set(dc_task_exec sample-task)
add_executable(${dc_task_exec} "src/main.cpp")
target_link_directories(${dc_task_exec} PUBLIC "${DDS_LIBDIR}")
target_link_libraries(${dc_task_exec} PUBLIC dds_intercom_lib)
target_include_directories(${dc_task_exec} PUBLIC "${DDS_INCDIR}" "${Boost_INCLUDE_DIRS}")

install(TARGETS "${dc_task_exec}" RUNTIME DESTINATION bin)