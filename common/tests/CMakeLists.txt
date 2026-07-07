#
# Copyright 2024 Centreon
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

include_directories(
  ${CMAKE_SOURCE_DIR}/common/inc
      )

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_executable(
    ut_common
    absl_flyweight_factory_test.cc
    crypto/base64.cc
    crypto/aes256.cc
    engine_conf/expand_conf.cc
    engine_conf/resolve_conf.cc
    engine_conf/indexed_diff_state.cc
    file_test.cc
    jwt_test.cc
    hex_dump_test.cc
    log_v2/log_v2.cc
    node_allocator_test.cc
    perfdata_test.cc
    process_stat_test.cc
    process_test.cc
    rapidjson_helper_test.cc
    test_main.cc
    timeperiods/utils.cc
    timeperiods/bam_legacy_timeperiods.cc
    timeperiods/calculate_time_from_day_of_month.cc
    timeperiods/calculate_time_from_weekday_of_month.cc
    timeperiods/get_next_invalid_time.cc
    timeperiods/get_next_valid_time/between_two_years.cc
    timeperiods/get_next_valid_time/calendar_date.cc
    timeperiods/get_next_valid_time/dst_backward.cc
    timeperiods/get_next_valid_time/dst_forward.cc
    timeperiods/get_next_valid_time/earliest_daterange_first.cc
    timeperiods/get_next_valid_time/exclusion.cc
    timeperiods/get_next_valid_time/generic_month_date.cc
    timeperiods/get_next_valid_time/normal_weekday.cc
    timeperiods/get_next_valid_time/offset_weekday_of_generic_month.cc
    timeperiods/get_next_valid_time/offset_weekday_of_specific_month.cc
    timeperiods/get_next_valid_time/precedence.cc
    timeperiods/get_next_valid_time/skip_interval.cc
    timeperiods/get_next_valid_time/specific_month_date.cc
    timeperiods/get_next_valid_time/timezone.cc
    utf8_test.cc
    ${TESTS_SOURCES})
else()
  add_executable(
    ut_common
    absl_flyweight_factory_test.cc
    perfdata_test.cc
    process_test.cc
    rapidjson_helper_test.cc
    test_main_win.cc
    utf8_test.cc
    ${TESTS_SOURCES})
endif()

set(TESTS_DIR "${PROJECT_SOURCE_DIR}/tests")

target_compile_definitions(
  ut_common PUBLIC -DHTTP_TEST_DIR="${PROJECT_SOURCE_DIR}/http/test"
                   -DCOMMON_CFG_TEST=\"${TESTS_DIR}/cfg_files\"
                   -DBAM_TP_CFG_DIR=\"${CMAKE_SOURCE_DIR}/broker/bam/test/time/cfg\")

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

file(COPY ${PROJECT_SOURCE_DIR}/tests/scripts/echo.bat
     DESTINATION ${CMAKE_BINARY_DIR}/tests)
file(COPY ${PROJECT_SOURCE_DIR}/tests/scripts/echo_env1_env2.bat
     DESTINATION ${CMAKE_BINARY_DIR}/tests)
file(COPY ${PROJECT_SOURCE_DIR}/tests/scripts/bad_script.bat
     DESTINATION ${CMAKE_BINARY_DIR}/tests)
file(COPY ${PROJECT_SOURCE_DIR}/tests/scripts/echo_env1_env2.sh
     DESTINATION ${CMAKE_BINARY_DIR}/tests)

add_test(NAME tests COMMAND ut_common)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  target_include_directories(ut_common PRIVATE ${CMAKE_SOURCE_DIR})

  target_link_libraries(
    ut_common
    PRIVATE centreon_common
            process_stat
            ctncrypto
            centreon_http
            centreon_process
            ctncrypto
            protobuf::libprotobuf
            engine_conf
            timeperiods
            Boost::program_options
            re2::re2
            log_v2
            OpenSSL::SSL
            OpenSSL::Crypto
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
            absl::any
            absl::log
            absl::base
            absl::bits
            fmt::fmt
            ${CMAKE_DL_LIBS}
            pthread)

  add_dependencies(ut_common centreon_common process_stat ctncrypto
                   centreon_http timeperiods)

else()
  target_link_libraries(
    ut_common
    PRIVATE centreon_common
            centreon_process
            ctncrypto
            Boost::program_options
            Boost::process
            gRPC::grpc++
            re2::re2
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
            absl::any
            absl::log
            absl::base
            absl::bits
            fmt::fmt)

  add_dependencies(ut_common centreon_common ctncrypto)

endif()

set_property(TARGET ut_common PROPERTY POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(ut_common PRIVATE
                          ${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh)
