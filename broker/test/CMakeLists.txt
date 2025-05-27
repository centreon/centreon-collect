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
include_directories(
  ${CMAKE_SOURCE_DIR}/bbdo
  ${CMAKE_BINARY_DIR}/bbdo
  ${PROJECT_SOURCE_DIR}/bam/inc
  ${PROJECT_SOURCE_DIR}/bam/test
  ${PROJECT_SOURCE_DIR}/storage/src
  ${PROJECT_SOURCE_DIR}/storage/inc
  ${PROJECT_SOURCE_DIR}/unified_sql/inc
  ${PROJECT_SOURCE_DIR}/graphite/inc
  ${PROJECT_SOURCE_DIR}/sql/inc
  ${PROJECT_SOURCE_DIR}/influxdb/inc
  ${PROJECT_SOURCE_DIR}/lua/inc
  ${PROJECT_SOURCE_DIR}/neb/inc
  ${PROJECT_SOURCE_DIR}/rrd/inc
  ${PROJECT_SOURCE_DIR}/simu/inc
  ${PROJECT_SOURCE_DIR}/stats/inc
  ${PROJECT_SOURCE_DIR}/tcp/inc
  ${PROJECT_SOURCE_DIR}/tls/inc
  ${PROJECT_SOURCE_DIR}/tls2/inc
  ${PROJECT_SOURCE_DIR}/grpc/inc
  ${PROJECT_SOURCE_DIR}/grpc/src
  ${CMAKE_SOURCE_DIR}/common/http/inc
  ${CMAKE_SOURCE_DIR}/common/grpc/inc
  ${CMAKE_SOURCE_DIR}/common/log_v2/inc
  ${PROJECT_SOURCE_DIR}/http_tsdb/inc
  ${PROJECT_SOURCE_DIR}/victoria_metrics/inc
  ${PROJECT_SOURCE_DIR}/test/test_util/inc)

add_subdirectory(${PROJECT_SOURCE_DIR}/test/test_util)

# needed by module.cc
add_definitions(
  -DCENTREON_BROKER_TEST_MODULE_PATH="${CMAKE_BINARY_DIR}/test/"
  -DCENTREON_BROKER_BAM_TEST_PATH="${BAM_TESTS_DIR}"
  -DCENTREON_BROKER_WD_TEST="${WDOG_TESTS_DIR}"
  -DCENTREON_BROKER_LUA_SCRIPT_PATH="${PROJECT_SOURCE_DIR}/lua/test"
  -DCENTREON_BROKER_NEB_TEST="${PROJECT_SOURCE_DIR}/neb/test")

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
  main.cc
  test_server.cc
  # Module sources.
  ${TESTS_SOURCES})

set_property(TARGET ut_broker PROPERTY ENABLE_EXPORTS ON)

target_link_libraries(
  ut_broker
  PRIVATE
          GTest::gtest
          GTest::gtest_main
          GTest::gmock
          GTest::gmock_main
	            log_v2
	  #	  test_util
	  #	  "-Wl,--whole-archive"
	            roker
	            rokerbase
	  	  centreon_common
		  #         multiplexing
	  #	  protobuf::libprotobuf
	  #	  "-Wl,--no-whole-archive"
	  #          ${TESTS_LIBRARIES}
	  #	  #conflictmgr
	  #          centreon_grpc
	  #	  #stdc++fs
	  #	  #nlohmann_json::nlohmann_json
          fmt::fmt
	  #	  #mariadb
	  #	  #crypto
	  #	  #ssl
	  	  gRPC::grpc++)

add_dependencies(
  ut_broker
  test_util
  roker
  rokerbase
  multiplexing
  conflictmgr
  centreon_common)

target_precompile_headers(ut_broker PRIVATE precomp_inc/precomp.hh)

set_target_properties(
  ut_broker rpc_client
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# keys used by ut_broker grpc
file(COPY ${PROJECT_SOURCE_DIR}/grpc/test/grpc_test_keys
     DESTINATION ${CMAKE_BINARY_DIR}/tests)

add_test(NAME tests COMMAND ut_broker)

#if(WITH_COVERAGE)
#  set(COVERAGE_EXCLUDES '*/main.cc' '*/test/*' '/usr/include/*'
#                        '${CMAKE_BINARY_DIR}/*' '*/.conan/*')
#  setup_target_for_coverage(NAME broker-test-coverage EXECUTABLE ut_broker
#                            DEPENDENCIES ut_broker)
#endif()
