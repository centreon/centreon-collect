#
# Copyright 2022-2025 Centreon
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

# Global options.

set(INC_DIR "${PROJECT_SOURCE_DIR}/http/inc/com/centreon/common/http")
set(SRC_DIR "src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/http/test")

# Sources.
set(SOURCES
  ${SRC_DIR}/http_client.cc
  ${SRC_DIR}/http_connection.cc
  ${SRC_DIR}/https_connection.cc
  ${SRC_DIR}/http_server.cc
)

add_library(centreon_http STATIC ${SOURCES})
target_include_directories(centreon_http PRIVATE ${INC_DIR})

target_precompile_headers(centreon_http REUSE_FROM centreon_common)

set_target_properties(centreon_http PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Testing.
if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES} ${TEST_DIR}/http_client_test.cc
      ${TEST_DIR}/http_connection_test.cc ${TEST_DIR}/http_server_test.cc
      ${TEST_DIR}/vault_test.cc
      PARENT_SCOPE)
endif(WITH_TESTING)
