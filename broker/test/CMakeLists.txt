#
# Copyright 2016-2023 Centreon
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

# Tests directory.
set(TESTS_DIR ${PROJECT_SOURCE_DIR}/core/test)
set(BAM_TESTS_DIR ${PROJECT_SOURCE_DIR}/bam/test)
set(WDOG_TESTS_DIR ${PROJECT_SOURCE_DIR}/watchdog/test)

# Include directories.
include_directories(${CMAKE_SOURCE_DIR}/bbdo)
include_directories(${PROJECT_SOURCE_DIR}/bam/inc)
include_directories(${PROJECT_SOURCE_DIR}/bam/test)
include_directories(${PROJECT_SOURCE_DIR}/storage/src)
include_directories(${PROJECT_SOURCE_DIR}/storage/inc)
include_directories(${PROJECT_SOURCE_DIR}/unified_sql/inc)
include_directories(${PROJECT_SOURCE_DIR}/graphite/inc)
include_directories(${PROJECT_SOURCE_DIR}/sql/inc)
include_directories(${PROJECT_SOURCE_DIR}/influxdb/inc)
include_directories(${PROJECT_SOURCE_DIR}/lua/inc)
include_directories(${PROJECT_SOURCE_DIR}/neb/inc)
include_directories(${PROJECT_SOURCE_DIR}/rrd/inc)
include_directories(${PROJECT_SOURCE_DIR}/simu/inc)
include_directories(${PROJECT_SOURCE_DIR}/stats/inc)
include_directories(${PROJECT_SOURCE_DIR}/tcp/inc)
include_directories(${PROJECT_SOURCE_DIR}/tls/inc)
include_directories(${PROJECT_SOURCE_DIR}/tls2/inc)
include_directories(${PROJECT_SOURCE_DIR}/grpc/inc)
include_directories(${PROJECT_SOURCE_DIR}/grpc/src)
include_directories(${CMAKE_SOURCE_DIR}/common/http/inc)
include_directories(${CMAKE_SOURCE_DIR}/common/grpc/inc)
include_directories(${CMAKE_SOURCE_DIR}/common/log_v2/inc)
include_directories(${PROJECT_SOURCE_DIR}/http_tsdb/inc)
include_directories(${PROJECT_SOURCE_DIR}/victoria_metrics/inc)
include_directories(${PROJECT_SOURCE_DIR}/test/test_util/inc)

add_subdirectory(${PROJECT_SOURCE_DIR}/test/test_util)

# needed by module.cc
add_definitions(-DCENTREON_BROKER_TEST_MODULE_PATH="${CMAKE_BINARY_DIR}/test/")
add_definitions(-DCENTREON_BROKER_BAM_TEST_PATH="${BAM_TESTS_DIR}")
add_definitions(-DCENTREON_BROKER_WD_TEST="${WDOG_TESTS_DIR}")
add_definitions(
  -DCENTREON_BROKER_LUA_SCRIPT_PATH="${PROJECT_SOURCE_DIR}/lua/test")
add_definitions(-DCENTREON_BROKER_NEB_TEST="${PROJECT_SOURCE_DIR}/neb/test")

add_library(null_module SHARED ${TEST_DIR}/modules/null_module.cc)
add_library(bad_version_module SHARED ${TEST_DIR}/modules/bad_version_module.cc)

# Unit test executable.
if(WITH_SQL_TESTS)
  set(TESTS_SOURCES ${TESTS_SOURCES} ${TEST_DIR}/mysql/mysql.cc)
endif(WITH_SQL_TESTS)

add_definitions(-DBROKERRPC_TESTS_PATH="${TESTS_DIR}/rpc")

add_executable(rpc_client ${TESTS_DIR}/rpc/client.cc)
add_dependencies(rpc_client centreon_common)

target_link_libraries(
  rpc_client
  berpc
  centreon_common
  spdlog::spdlog
  gRPC::gpr
  gRPC::grpc
  gRPC::grpc++
  gRPC::grpc++_alts
  crypto
  ssl
  z
  dl
  pthread)

if(WITH_ASAN)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS_DEBUG
      "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

add_executable(
  ut_broker
  # Core sources.
  ${TESTS_DIR}/bbdo/category.cc
  ${TESTS_DIR}/bbdo/output.cc
  ${TESTS_DIR}/bbdo/read.cc
  ${TESTS_DIR}/cache/global_cache_test.cc
  ${TESTS_DIR}/compression/stream/memory_stream.hh
  ${TESTS_DIR}/compression/stream/read.cc
  ${TESTS_DIR}/compression/stream/write.cc
  ${TESTS_DIR}/compression/zlib/zlib.cc
  ${TESTS_DIR}/config/init.cc
  ${TESTS_DIR}/config/parser.cc
  ${TESTS_DIR}/file/disk_accessor.cc
  ${TESTS_DIR}/file/splitter/concurrent.cc
  ${TESTS_DIR}/file/splitter/default.cc
  ${TESTS_DIR}/file/splitter/more_than_max_size.cc
  ${TESTS_DIR}/file/splitter/permission_denied.cc
  ${TESTS_DIR}/file/splitter/resume.cc
  ${TESTS_DIR}/file/splitter/split.cc
  ${TESTS_DIR}/file/splitter/split_limited.cc
  ${TESTS_DIR}/misc/buffer.cc
  ${TESTS_DIR}/misc/exec.cc
  ${TESTS_DIR}/misc/filesystem.cc
  ${TESTS_DIR}/misc/math.cc
  ${TESTS_DIR}/misc/misc.cc
  ${TESTS_DIR}/misc/string.cc
  ${TESTS_DIR}/modules/module.cc
  ${TESTS_DIR}/processing/acceptor.cc
  ${TESTS_DIR}/processing/feeder.cc
  ${TESTS_DIR}/time/timerange.cc
  ${TESTS_DIR}/rpc/brokerrpc.cc
  ${TESTS_DIR}/exceptions.cc
  ${TESTS_DIR}/io.cc
  ${TESTS_DIR}/main.cc
  ${TESTS_DIR}/test_server.cc
  # Module sources.
  ${TESTS_SOURCES})

message(STATUS "###################################")
message(STATUS "###################################")
message(STATUS ${TESTS_SOURCES})
message(STATUS "###################################")
message(STATUS "###################################")

target_link_libraries(
  ut_broker
  PRIVATE test_util
          roker
          rokerbase
          multiplexing
          ${TESTS_LIBRARIES}
          conflictmgr
          centreon_common
          centreon_grpc
          stdc++fs
          nlohmann_json::nlohmann_json
          fmt::fmt
          log_v2
          GTest::gtest
          GTest::gtest_main
          GTest::gmock
          GTest::gmock_main
          mariadb
          crypto
          ssl
          gRPC::gpr
          gRPC::grpc
          gRPC::grpc++
          gRPC::grpc++_alts)

add_dependencies(
  ut_broker
  test_util
  roker
  rokerbase
  multiplexing
  conflictmgr
  centreon_common)

set_target_properties(ut_broker PROPERTIES COMPILE_FLAGS "-fPIC")

target_precompile_headers(ut_broker REUSE_FROM rokerbase)

set_target_properties(
  ut_broker rpc_client
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
             RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)

# keys used by ut_broker grpc
file(COPY ${PROJECT_SOURCE_DIR}/grpc/test/grpc_test_keys
     DESTINATION ${CMAKE_BINARY_DIR}/tests)

add_test(NAME tests COMMAND ut_broker)

if(WITH_COVERAGE)
  set(COVERAGE_EXCLUDES '*/main.cc' '*/test/*' '/usr/include/*'
                        '${CMAKE_BINARY_DIR}/*' '*/.conan/*')
  setup_target_for_coverage(NAME broker-test-coverage EXECUTABLE ut_broker
                            DEPENDENCIES ut_broker)
endif()
