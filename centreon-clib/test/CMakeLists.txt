#
# Copyright 2018-2022 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY # KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

find_package(GTest REQUIRED)
include_directories(${GTest_INCLUDE_DIRS})
link_directories(${GTest_LIB_DIRS})
add_executable(bin_test_process_output
               ${PROJECT_SOURCE_DIR}/test/bin_test_process_output.cc)

add_library(shared_testing_library SHARED
            ${PROJECT_SOURCE_DIR}/test/shared_testing_library.cc)

include_directories(${PROJECT_SOURCE_DIR}/test/)

add_executable(
  ut_clib
  ${PROJECT_SOURCE_DIR}/test/process.cc
  ${PROJECT_SOURCE_DIR}/test/exceptions.cc
  ${PROJECT_SOURCE_DIR}/test/handle_manager.cc
  ${PROJECT_SOURCE_DIR}/test/io.cc
  ${PROJECT_SOURCE_DIR}/test/library.cc
  ${PROJECT_SOURCE_DIR}/test/logging.cc
  ${PROJECT_SOURCE_DIR}/test/main.cc
  ${PROJECT_SOURCE_DIR}/test/misc.cc
  ${PROJECT_SOURCE_DIR}/test/task_manager.cc
  ${PROJECT_SOURCE_DIR}/test/timestamp.cc
  ${PROJECT_SOURCE_DIR}/test/version.cc)

set_target_properties(
  shared_testing_library
  PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
             LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
             LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
             LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
             LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)
set_target_properties(
  ut_clib bin_test_process_output
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)

if(WITH_COVERAGE)
  set(COVERAGE_EXCLUDES '${PROJECT_BINARY_DIR}/*'
                        '${PROJECT_SOURCE_DIR}/tests/*' '/usr/include/* ')
  setup_target_for_coverage(NAME test-coverage EXECUTABLE ut_clib DEPENDENCIES
                            ut_clib)
  set(GCOV gcov)
endif()

add_test(NAME tests COMMAND ut_clib)
target_link_libraries(ut_clib CONAN_PKG::gtest pthread centreon_clib)
