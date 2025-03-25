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

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_executable(
    ut_common
    absl_flyweight_factory_test.cc
    crypto/base64.cc
    engine_conf/expand_conf.cc
    file_test.cc
    hex_dump_test.cc
    log_v2/log_v2.cc
    node_allocator_test.cc
    perfdata_test.cc
    process_stat_test.cc
    process_test.cc
    rapidjson_helper_test.cc
    test_main.cc
    utf8_test.cc
    ${TESTS_SOURCES})
else()
  add_executable(ut_common
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
  )

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
file(COPY ${PROJECT_SOURCE_DIR}/tests/scripts/bad_script.bat
     DESTINATION ${CMAKE_BINARY_DIR}/tests)

add_test(NAME tests COMMAND ut_common)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  target_link_libraries(
    ut_common
    PRIVATE centreon_common
            engine_conf
            ctncrypto
            centreon_http
            centreon_process
            -L${Boost_LIBRARY_DIR_RELEASE}
            boost_program_options
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
            pthread)

  add_dependencies(ut_common centreon_common centreon_http)

else()
  target_link_libraries(
    ut_common
    PRIVATE centreon_common
            centreon_process
            Boost::program_options
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

  add_dependencies(ut_common centreon_common)

endif()

set_property(TARGET ut_common PROPERTY POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(ut_common PRIVATE
                          ${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh)
