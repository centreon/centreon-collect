#
# Copyright 2021-2023 Centreon
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
set(INC_DIR "${PROJECT_SOURCE_DIR}/unified_sql/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/unified_sql/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/unified_sql/test")
include_directories("${INC_DIR}" "${SRC_DIR}"
  ${CMAKE_BINARY_DIR}/common/process_stat
"${PROJECT_SOURCE_DIR}/neb/inc"
"${CMAKE_SOURCE_DIR}")
set(INC_DIR "${INC_DIR}/com/centreon/broker/unified_sql")

# Storage module.
set(UNIFIED_SQL "20-unified_sql")
set(UNIFIED_SQL
    "${UNIFIED_SQL}"
    PARENT_SCOPE)
add_library(
  "${UNIFIED_SQL}" SHARED
  # Sources.
  ${SRC_DIR}/bulk_bind.cc
  ${SRC_DIR}/bulk_queries.cc
  ${SRC_DIR}/connector.cc
  ${SRC_DIR}/database_configurator.cc
  ${SRC_DIR}/factory.cc
  ${SRC_DIR}/main.cc
  ${SRC_DIR}/rebuilder.cc
  ${SRC_DIR}/stored_timestamp.cc
  ${SRC_DIR}/stream.cc
  ${SRC_DIR}/stream_sql.cc
  ${SRC_DIR}/stream_storage.cc
  # Headers.
  ${INC_DIR}/bulk_queries.hh
  ${INC_DIR}/bulk_bind.hh
  ${INC_DIR}/connector.hh
  ${INC_DIR}/database_configurator.hh
  ${INC_DIR}/events.hh
  ${INC_DIR}/factory.hh
  ${INC_DIR}/internal.hh
  ${INC_DIR}/rebuilder.hh
  ${INC_DIR}/stored_timestamp.hh
  ${INC_DIR}/stream.hh)
set_target_properties("${UNIFIED_SQL}" PROPERTIES PREFIX ""
  POSITION_INDEPENDENT_CODE TRUE
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")
add_dependencies("${UNIFIED_SQL}" pb_rebuild_message_lib
                 pb_remove_graph_message_lib
		 process_stat
                 pb_neb_lib
                 pb_open_telemetry_lib)
target_precompile_headers(${UNIFIED_SQL} PRIVATE precomp_inc/precomp.hpp)
target_link_libraries(
  ${UNIFIED_SQL} PRIVATE
  bbdo_storage
  pb_storage_lib
  pb_rebuild_message_lib
  pb_remove_graph_message_lib
  spdlog::spdlog
  engine_conf
  absl::any absl::log absl::base absl::bits)

# Testing.
if(WITH_TESTING)
  if(WITH_SQL_TESTS)
    set(TEST_SOURCES ${TESTS_SOURCES} ${TEST_DIR}/conflict_manager.cc)
  endif(WITH_SQL_TESTS)

  # Testing.
  set(TESTS_SOURCES
      ${TESTS_SOURCES}
      ${TEST_DIR}/connector.cc
      ${TEST_DIR}/metric.cc
      ${TEST_DIR}/rebuild_message.cc
      ${TEST_DIR}/remove_graph.cc
      ${TEST_DIR}/status.cc
      ${TEST_DIR}/status-entry.cc
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} ${UNIFIED_SQL}
      PARENT_SCOPE)
endif()

# Install rule.
install(TARGETS "${UNIFIED_SQL}" LIBRARY DESTINATION "${PREFIX_MODULES}")
