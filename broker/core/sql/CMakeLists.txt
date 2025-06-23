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

#
# Global settings.
#

set(TESTS_DIR "${PROJECT_SOURCE_DIR}/core/sql/test")

add_custom_command(
  DEPENDS ${PROJECT_SOURCE_DIR}/core/sql/src/table_max_size.py
          "${CMAKE_SOURCE_DIR}/resources/centreon.sql"
          "${CMAKE_SOURCE_DIR}/resources/centreon_storage.sql"
  COMMENT "Generating table_max_size.hh"
  OUTPUT
    "${PROJECT_SOURCE_DIR}/core/sql/inc/com/centreon/broker/sql/table_max_size.hh"
  COMMAND
    python3 ARGS "${PROJECT_SOURCE_DIR}/core/sql/src/table_max_size.py"
    "${PROJECT_SOURCE_DIR}/core/sql/inc/com/centreon/broker/sql/table_max_size.hh"
    "${CMAKE_SOURCE_DIR}/resources/centreon.sql"
    "${CMAKE_SOURCE_DIR}/resources/centreon_storage.sql")

add_custom_target(table_max_size
                  DEPENDS inc/com/centreon/broker/sql/table_max_size.hh)

set_source_files_properties(inc/com/centreon/broker/sql/table_max_size.hh
                            PROPERTIES GENERATED TRUE)

# Sources.
set(SOURCES
    # Sources
    src/database_config.cc
    src/query_preparator.cc
    src/mysql.cc
    src/mysql_manager.cc
    src/mysql_connection.cc
    src/mysql_bind_base.cc
    src/mysql_bind.cc
    src/mysql_bind_result.cc
    src/mysql_bulk_bind.cc
    src/mysql_column.cc
    src/mysql_result.cc
    src/mysql_stmt_base.cc
    src/mysql_stmt.cc
    src/mysql_bulk_stmt.cc
    src/mysql_multi_insert.cc
    src/stats.cc
    # Headers
    inc/com/centreon/broker/sql/database_config.hh
    inc/com/centreon/broker/sql/mysql.hh
    inc/com/centreon/broker/sql/mysql_bulk_bind.hh
    inc/com/centreon/broker/sql/mysql_bulk_stmt.hh
    inc/com/centreon/broker/sql/mysql_column.hh
    inc/com/centreon/broker/sql/mysql_connection.hh
    inc/com/centreon/broker/sql/mysql_error.hh
    inc/com/centreon/broker/sql/mysql_manager.hh
    inc/com/centreon/broker/sql/mysql_result.hh
    inc/com/centreon/broker/sql/mysql_stmt.hh
    inc/com/centreon/broker/sql/mysql_stmt_base.hh
    inc/com/centreon/broker/sql/mysql_task.hh
    inc/com/centreon/broker/sql/query_preparator.hh
    inc/com/centreon/broker/sql/stats.hh
    inc/com/centreon/broker/sql/table_max_size.hh)

# Static libraries.
add_library(sql STATIC ${SOURCES})
set_target_properties(sql PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(sql PRIVATE ../precomp_inc/precomp.hpp)
target_link_libraries(sql PRIVATE spdlog::spdlog mariadb centreon_http)
add_dependencies(sql process_stat pb_common_lib)

if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES} ${TESTS_DIR}/engine/start_stop.cc
      ${TESTS_DIR}/muxer/read.cc ${TESTS_DIR}/publisher/read.cc
      ${TESTS_DIR}/publisher/write.cc)
endif(WITH_TESTING)
