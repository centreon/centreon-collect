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

set( SRC_COMMON
    check_test.cc 
    check_exec_test.cc
    drive_size_test.cc
    check_health_test.cc
    scheduler_test.cc
    test_main.cc
)



if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(SRC ${SRC_COMMON} config_test.cc check_linux_cpu_test.cc)
else()
  set(SRC ${SRC_COMMON}
    filter_test.cc
    check_event_log_data_test.cc
    check_event_log_filter_test.cc
    check_event_log_test.cc
    check_event_log_uniq_test.cc
    check_process_container_test.cc
    check_process_data_test.cc
    check_process_filter_test.cc
    check_process_test.cc
    check_windows_cpu_test.cc  
    check_windows_counter.cc
    check_windows_memory_test.cc 
    check_windows_sched.cc
    check_uptime_test.cc 
    check_windows_service_test.cc)

  set_source_files_properties(filter_test.cc PROPERTIES COMPILE_FLAGS /bigobj)
endif()


add_executable(ut_agent ${SRC})

add_test(NAME tests COMMAND ut_agent)

set_target_properties(
    ut_agent
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(ut_agent PRIVATE
        centagent_lib
        centreon_common
        centreon_process
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
	Boost::program_options
        stdc++fs
        gRPC::grpc++
        fmt::fmt pthread
        crypto ssl
        )
else()
    target_link_libraries(ut_agent PRIVATE
    centagent_lib
    centreon_common
    centreon_process
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
    Boost::program_options
    gRPC::grpc++
    fmt::fmt
    pdh
    taskschd)
endif()

add_dependencies(ut_agent centreon_common centagent_lib)

set_property(TARGET ut_agent PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(ut_agent PRIVATE "${PROJECT_SOURCE_DIR}/src")

target_precompile_headers(ut_agent PRIVATE ${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh)

file(COPY ${PROJECT_SOURCE_DIR}/test/scripts/sleep.bat
     DESTINATION ${CMAKE_BINARY_DIR}/tests)

