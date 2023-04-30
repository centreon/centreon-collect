# Copyright 2023 Centreon
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
set(INC_DIR "${PROJECT_SOURCE_DIR}/stats_exporter/inc/com/centreon/broker/stats_exporter")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/stats_exporter/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/stats_exporter/test")
include_directories("${PROJECT_SOURCE_DIR}/stats_exporter/inc")
include_directories("${PROJECT_SOURCE_DIR}/neb/inc")

# Stats module.
set(STATS_EXPORTER "15-stats_exporter")
set(STATS_EXPORTER "${STATS_EXPORTER}" PARENT_SCOPE)
add_library("${STATS_EXPORTER}" SHARED

  # Sources.
  ${SRC_DIR}/builder.cc
  ${SRC_DIR}/main.cc
  ${SRC_DIR}/parser.cc
  ${SRC_DIR}/worker.cc
  ${SRC_DIR}/worker_pool.cc

  # Headers.
  ${INC_DIR}/builder.hh
  ${INC_DIR}/parser.hh
  ${INC_DIR}/worker.hh
  ${INC_DIR}/worker_pool.hh
)
set_target_properties("${STATS_EXPORTER}" PROPERTIES
  PREFIX "")
target_link_libraries("${STATS_EXPORTER}" CONAN_PKG::spdlog)
target_precompile_headers(${STATS_EXPORTER} PRIVATE precomp_inc/precomp.hpp)

add_dependencies(${STATS_EXPORTER} target_broker_message)

# Testing.
if(WITH_TESTING)
  set(
    TESTS_SOURCES
    ${TESTS_SOURCES}
    ${TEST_DIR}/stats.cc
    PARENT_SCOPE
  )
  set(
    TESTS_LIBRARIES
    ${TESTS_LIBRARIES}
    ${STATS_EXPORTER}
    PARENT_SCOPE
  )
endif(WITH_TESTING)

# Install rule.
install(TARGETS "${STATS_EXPORTER}"
  LIBRARY DESTINATION "${PREFIX_MODULES}"
)
