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



add_executable(ut_agent 
    config_test.cc
    test_main.cc
    ${TESTS_SOURCES})

add_test(NAME tests COMMAND ut_agent)

set_target_properties(
    ut_agent
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)


target_link_libraries(ut_agent PRIVATE 
    centagent_lib 
    centreon_common
    GTest::gtest 
    GTest::gtest_main 
    GTest::gmock 
    GTest::gmock_main
    -L${Boost_LIBRARY_DIR_RELEASE}
    boost_program_options
    stdc++fs
    -L${PROTOBUF_LIB_DIR}
    gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
    fmt::fmt pthread
    crypto ssl
    )

add_dependencies(ut_agent centreon_common centagent_lib)

set_property(TARGET ut_agent PROPERTY POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(ut_agent PRIVATE ${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh)

