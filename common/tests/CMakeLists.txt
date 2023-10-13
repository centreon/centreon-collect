#
# Copyright 2023 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

include_directories(${CONAN_INCLUDE_DIRS_GTEST})

add_executable(ut_common main.cc log_v2/log_v2.cc)
set_target_properties(
  ut_common
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)

if(WITH_COVERAGE)
  set(COVERAGE_EXCLUDES '${PROJECT_SOURCE_DIR}/test/*' '/usr/include/*')
  setup_target_for_coverage(NAME clib-test-coverage EXECUTABLE ut_clib
                            DEPENDENCIES ut_clib)
  set(GCOV gcov)
endif()
add_test(NAME tests COMMAND ut_common)
target_link_libraries(ut_common CONAN_PKG::gtest pthread log_v2)
