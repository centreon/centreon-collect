##
## Copyright 2011-2013 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##

# Set directories.
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")

# Add subdirectories.
add_subdirectory("clib")
add_subdirectory("exceptions")
add_subdirectory("io")
add_subdirectory("logging")
add_subdirectory("misc")

# bin_test_process_output
set(TEST_NAME "bin_test_process_output")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")

# shared_testing_library
set(LIB_NAME "shared_testing_library")
add_library("${LIB_NAME}" SHARED "${TEST_DIR}/${LIB_NAME}.cc")

set(CLIB_TESTS
    handle_manager_add
    handle_manager_constructor
    handle_manager_remove_by_handle
    handle_manager_remove_by_handle_listener
    handle_manager_multiplex
    library_resolve
    library_load
    process_env
    process_kill
    process_return
    process_terminate
    process_timeout
    process_wait_timeout
   )

foreach (TEST_NAME ${CLIB_TESTS})
  add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
  target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
endforeach ()

# process_output
set(TEST_NAME "process_output")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}_stderr" "${TEST_NAME}" "err")
add_test("${TEST_NAME}_stdout" "${TEST_NAME}" "out")

# task_manager tests
set(TASK_MANAGER_LIST
    task_manager_add
    task_manager_add_recurring
    task_manager_execute
    task_manager_execute_recurring
    task_manager_next_execution_time
    task_manager_remove_by_id
    task_manager_remove_by_task
    task_manager_remove_self
   )

foreach(name ${TASK_MANAGER_LIST})
  add_executable("${name}" "${TEST_DIR}/${name}.cc")
  target_link_libraries("${name}" "${DEFAULT_LINK_NAME}")
  add_test("${name}" "${name}")
endforeach(name)

# task_manager tests
set(TIMESTAMP_LIST
    timestamp_add
    timestamp_add_mseconds
    timestamp_add_seconds
    timestamp_add_useconds
    timestamp_clear
    timestamp_constructor
    timestamp_equal
    timestamp_not_equal
    timestamp_copy
    timestamp_greater
    timestamp_greater_or_equal
    timestamp_less_or_equal
    timestamp_less
    timestamp_sub
    timestamp_sub_mseconds
    timestamp_sub_seconds
    timestamp_sub_useconds
    timestamp_to_mseconds
    timestamp_to_seconds
    timestamp_to_useconds
   )

foreach(name ${TIMESTAMP_LIST})
  add_executable("${name}" "${TEST_DIR}/${name}.cc")
  target_link_libraries("${name}" "${DEFAULT_LINK_NAME}")
  add_test("${name}" "${name}")
endforeach(name)

if (WITH_COVERAGE)
  include("${PROJECT_SOURCE_DIR}/cmake/CodeCoverage.cmake")
  APPEND_COVERAGE_COMPILER_FLAGS()
  set(COVERAGE_EXCLUDES '${PROJECT_BINARY_DIR}/*' '${PROJECT_SOURCE_DIR}/test/*' '/usr/include/*')
  SETUP_TARGET_FOR_COVERAGE(
      NAME test-coverage
      EXECUTABLE ctest # Executable in PROJECT_BINARY_DIR
      DEPENDENCIES
      ${CLIB_TESTS}
      ${TASK_MANAGER_LIST}
      ${TIMESTAMP_LIST}
      )
endif ()
