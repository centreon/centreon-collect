#
# Copyright 2022 Centreon
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
set(INC_DIR "inc/com/centreon/broker/http_client")
set(SRC_DIR "src")
set(TEST_DIR "test")
include_directories(${INC_DIR})

# Sources.
set(SOURCES
  ${SRC_DIR}/http_client.cc
  ${SRC_DIR}/http_connection.cc
)

# Headers.
set(HEADERS
  ${INC_DIR}/http_client.hh
  ${INC_DIR}/http_connection.hh
  ${INC_DIR}/http_config.hh
)

add_library(http_client STATIC ${SOURCES} ${HEADERS})
target_include_directories(http_client PRIVATE ${INC_DIR})

target_precompile_headers(http_client PRIVATE precomp_inc/precomp.hh)

# Testing.
# if(WITH_TESTING)
# set(TESTS_SOURCES
# ${TESTS_SOURCES} ${TEST_DIR}/acceptor.cc ${TEST_DIR}/channel_test.cc
# ${TEST_DIR}/stream_test.cc ${TEST_DIR}/factory_test.cc
# PARENT_SCOPE)
# endif(WITH_TESTING)
