#
# Copyright 2023-2025 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

# Global options.
set(INC_DIR "${PROJECT_SOURCE_DIR}/victoria_metrics/inc/com/centreon/broker/victoria_metrics")
set(HTTP_INC_DIR "${CMAKE_SOURCE_DIR}/common/http/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/victoria_metrics/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/victoria_metrics/test")
include_directories(inc
  ${PROJECT_SOURCE_DIR}/http_tsdb/inc
  ${HTTP_INC_DIR}
  ${PROJECT_SOURCE_DIR}/neb/inc
)

# Sources.
set(SOURCES
  ${SRC_DIR}/connector.cc
  ${SRC_DIR}/factory.cc
  ${SRC_DIR}/request.cc
  ${SRC_DIR}/stream.cc
  ${SRC_DIR}/main.cc
)

# Headers.
set(HEADERS
  ${INC_DIR}/connector.hh
  ${INC_DIR}/factory.hh
  ${INC_DIR}/request.hh
  ${INC_DIR}/stream.hh
)

# TCP module.
set(VICTORIA_METRICS "70-victoria_metrics")
set(VICTORIA_METRICS "${VICTORIA_METRICS}" PARENT_SCOPE)
add_library("${VICTORIA_METRICS}" SHARED
  ${SOURCES}
  ${HEADERS}
)
set_target_properties("${VICTORIA_METRICS}" PROPERTIES PREFIX ""
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")
target_link_libraries("${VICTORIA_METRICS}" PRIVATE
  http_tsdb
  protobuf::libprotobuf
  centreon_http
  bbdo_storage pb_storage_lib spdlog::spdlog)
target_precompile_headers(${VICTORIA_METRICS} PRIVATE precomp_inc/precomp.hh)

# Testing.
if(WITH_TESTING)
  set(
    TESTS_SOURCES
    ${TESTS_SOURCES}
    ${TEST_DIR}/factory_test.cc
    ${TEST_DIR}/stream_test.cc
    ${TEST_DIR}/request_test.cc
    PARENT_SCOPE
  )
  set(
    TESTS_LIBRARIES
    ${TESTS_LIBRARIES}
    ${VICTORIA_METRICS}
    PARENT_SCOPE
  )
endif(WITH_TESTING)

# Install rule.
install(TARGETS "${VICTORIA_METRICS}"
  LIBRARY DESTINATION "${PREFIX_MODULES}"
)
