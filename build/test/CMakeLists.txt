##
## Copyright 2011 Merethis
##
## This file is part of Centreon Clib.
##
## Centreon Clib is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Clib is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Clib. If not, see
## <http://www.gnu.org/licenses/>.
##

# Set directories.
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")

# Add subdirectories.
add_subdirectory("concurrency")
add_subdirectory("exception")
add_subdirectory("io")
add_subdirectory("logging")
add_subdirectory("misc")

# handle_copy
set(TEST_NAME "handle_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# handle_manager_add
set(TEST_NAME "handle_manager_add")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# handle_manager_constructor
set(TEST_NAME "handle_manager_constructor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# handle_manager_remove_by_handle
set(TEST_NAME "handle_manager_remove_by_handle")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# handle_manager_remove_by_handle_listener
set(TEST_NAME "handle_manager_remove_by_handle_listener")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# handle_manager_multiplex
set(TEST_NAME "handle_manager_multiplex")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# task_manager_add
set(TEST_NAME "task_manager_add")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# task_manager_add_recurring
set(TEST_NAME "task_manager_add_recurring")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# task_manager_remove_by_task
set(TEST_NAME "task_manager_remove_by_task")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# task_manager_remove_by_id
set(TEST_NAME "task_manager_remove_by_id")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# task_manager_next_execution_time
set(TEST_NAME "task_manager_next_execution_time")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# task_manager_execute
set(TEST_NAME "task_manager_execute")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# task_manager_execute_recurring
set(TEST_NAME "task_manager_execute_recurring")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_add
set(TEST_NAME "timestamp_add")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_add_msecond
set(TEST_NAME "timestamp_add_msecond")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_add_second
set(TEST_NAME "timestamp_add_second")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_add_usecond
set(TEST_NAME "timestamp_add_usecond")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_constructor
set(TEST_NAME "timestamp_constructor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_equal
set(TEST_NAME "timestamp_equal")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_not_equal
set(TEST_NAME "timestamp_not_equal")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_copy
set(TEST_NAME "timestamp_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_greater
set(TEST_NAME "timestamp_greater")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_greater_or_equal
set(TEST_NAME "timestamp_greater_or_equal")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_less_or_equal
set(TEST_NAME "timestamp_less_or_equal")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_less
set(TEST_NAME "timestamp_less")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_sub
set(TEST_NAME "timestamp_sub")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_sub_msecond
set(TEST_NAME "timestamp_sub_msecond")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_sub_second
set(TEST_NAME "timestamp_sub_second")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_sub_usecond
set(TEST_NAME "timestamp_sub_usecond")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_to_msecond
set(TEST_NAME "timestamp_to_msecond")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_to_second
set(TEST_NAME "timestamp_to_second")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# timestamp_to_usecond
set(TEST_NAME "timestamp_to_usecond")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
