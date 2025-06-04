#
# Copyright 2016, 2020-2024 Centreon
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

# Enable unit tests or not .
if(WITH_TESTING)
  # Tests directory. Add root inclusion direction.
  set(MODULE_DIR "${PROJECT_SOURCE_DIR}/modules/external_commands")
  set(INC_DIR "${MODULE_DIR}/inc/com/centreon/engine/modules/external_commands")
  set(MODULE_DIR_OTL "${PROJECT_SOURCE_DIR}/modules/opentelemetry")

  include_directories(
    ${PROJECT_SOURCE_DIR} ${MODULE_DIR}/inc ${MODULE_DIR_OTL}/inc
    ${CMAKE_SOURCE_DIR}/bbdo ${CMAKE_SOURCE_DIR}/common/http/inc)

  # Set directory.
  set(TESTS_DIR "${PROJECT_SOURCE_DIR}/tests")
  include_directories(${PROJECT_SOURCE_DIR}/enginerpc)
  add_definitions("-DENGINERPC_TESTS_PATH=${TESTS_DIR}/enginerpc"
                  "-DENGINE_CFG_TEST=\"${TESTS_DIR}/cfg_files\"")

  add_executable(rpc_client_engine ${TESTS_DIR}/enginerpc/client.cc)

  target_link_libraries(
    rpc_client_engine
    PRIVATE cerpc
    centreon_common
    process_stat
    spdlog::spdlog
    gRPC::grpc++
    crypto
    ssl
    z
    dl
    pthread)

  add_executable(bin_connector_test_run
                 "${TESTS_DIR}/commands/bin_connector_test_run.cc")
  target_link_libraries(bin_connector_test_run cce_core pthread)
  target_precompile_headers(bin_connector_test_run REUSE_FROM cce_core)

  # Unit test executable.
  include_directories(${TESTS_DIR} ${CMAKE_BINARY_DIR})

  if(WITH_ASAN)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG
        "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address"
    )
  endif()

  # file used by timeperiod-test.cc
  file(COPY ${TESTS_DIR}/configuration/timeperiods.cfg
       DESTINATION ${CMAKE_BINARY_DIR}/tests/)

  set(ut_sources
      # Sources.
      ${TESTS_DIR}/parse-check-output.cc
      ${TESTS_DIR}/checks/pb_service_check.cc
      ${TESTS_DIR}/checks/pb_service_retention.cc
      ${TESTS_DIR}/checks/pb_anomalydetection.cc
      ${TESTS_DIR}/commands/pbsimple-command.cc
      ${TESTS_DIR}/commands/connector.cc
      ${TESTS_DIR}/commands/environment.cc
      ${TESTS_DIR}/configuration/applier/applier-pbanomalydetection.cc
      ${TESTS_DIR}/configuration/applier/applier-pbcommand.cc
      ${TESTS_DIR}/configuration/applier/applier-pbconnector.cc
      ${TESTS_DIR}/configuration/applier/applier-pbcontact.cc
      ${TESTS_DIR}/configuration/applier/applier-pbcontactgroup.cc
      ${TESTS_DIR}/configuration/applier/applier-pbglobal.cc
      ${TESTS_DIR}/configuration/applier/applier-pblog.cc
      ${TESTS_DIR}/configuration/applier/applier-pbhost.cc
      ${TESTS_DIR}/configuration/applier/applier-pbhostescalation.cc
      ${TESTS_DIR}/configuration/applier/applier-pbhostdependency.cc
      ${TESTS_DIR}/configuration/applier/applier-pbhostgroup.cc
      ${TESTS_DIR}/configuration/applier/applier-pbservice.cc
      ${TESTS_DIR}/configuration/applier/applier-pbserviceescalation.cc
      ${TESTS_DIR}/configuration/applier/applier-pbservicegroup.cc
      ${TESTS_DIR}/configuration/applier/applier-pbstate.cc
      ${TESTS_DIR}/configuration/pbcontact.cc
      ${TESTS_DIR}/configuration/pbhost.cc
      ${TESTS_DIR}/configuration/pbservice.cc
      ${TESTS_DIR}/configuration/pbseverity.cc
      ${TESTS_DIR}/configuration/pbtag.cc
      ${TESTS_DIR}/configuration/pbtimeperiod-test.cc
      ${TESTS_DIR}/configuration/whitelist-test.cc
      ${TESTS_DIR}/contacts/contactgroup-config.cc
      ${TESTS_DIR}/contacts/simple-contactgroup.cc
      ${TESTS_DIR}/custom_vars/pbextcmd.cc
      ${TESTS_DIR}/downtimes/pbdowntime.cc
      ${TESTS_DIR}/downtimes/pbdowntime_finder.cc
      ${TESTS_DIR}/enginerpc/pbenginerpc.cc
      ${TESTS_DIR}/helper.cc
      ${TESTS_DIR}/macros/pbmacro.cc
      ${TESTS_DIR}/macros/pbmacro_hostname.cc
      ${TESTS_DIR}/macros/pbmacro_service.cc
      ${TESTS_DIR}/external_commands/pbanomalydetection.cc
      ${TESTS_DIR}/external_commands/pbhost.cc
      ${TESTS_DIR}/external_commands/pbservice.cc
      ${TESTS_DIR}/main.cc
      ${TESTS_DIR}/loop/loop.cc
      ${TESTS_DIR}/notifications/host_downtime_notification.cc
      ${TESTS_DIR}/notifications/host_flapping_notification.cc
      ${TESTS_DIR}/notifications/host_normal_notification.cc
      ${TESTS_DIR}/notifications/host_recovery_notification.cc
      ${TESTS_DIR}/notifications/service_normal_notification.cc
      ${TESTS_DIR}/notifications/service_timeperiod_notification.cc
      ${TESTS_DIR}/notifications/service_flapping_notification.cc
      ${TESTS_DIR}/notifications/service_downtime_notification_test.cc
      ${TESTS_DIR}/opentelemetry/grpc_config_test.cc
      ${TESTS_DIR}/opentelemetry/host_serv_extractor_test.cc
      ${TESTS_DIR}/opentelemetry/otl_server_test.cc
      ${TESTS_DIR}/opentelemetry/otl_converter_test.cc
      ${TESTS_DIR}/opentelemetry/open_telemetry_test.cc
      ${TESTS_DIR}/retention/host.cc
      ${TESTS_DIR}/retention/service.cc
      ${TESTS_DIR}/string/string.cc
      ${TESTS_DIR}/test_engine.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/between_two_years.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/calendar_date.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/dst_backward.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/dst_forward.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/earliest_daterange_first.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/exclusion.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/exceptions_test.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/generic_month_date.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/normal_weekday.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/offset_weekday_of_generic_month.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/offset_weekday_of_specific_month.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/precedence.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/skip_interval.cc
      ${TESTS_DIR}/timeperiod/get_next_valid_time/specific_month_date.cc
      # Headers.
      "${TESTS_DIR}/test_engine.hh"
      "${TESTS_DIR}/timeperiod/utils.hh")
  add_library(ut_engine_utils STATIC "${TESTS_DIR}/timeperiod/utils.cc")
  target_link_libraries(ut_engine_utils PRIVATE dl)
  add_executable(ut_engine ${ut_sources})
  target_include_directories(
    ut_engine
    PRIVATE ${MODULE_DIR_OTL}/src ${CMAKE_SOURCE_DIR}/common/grpc/inc
            ${CMAKE_SOURCE_DIR}/agent/inc ${CMAKE_SOURCE_DIR}/agent/src)

  target_precompile_headers(ut_engine REUSE_FROM cce_core)

  set_target_properties(
    ut_engine rpc_client_engine bin_connector_test_run
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)

  # Ensure ut_engine depends on ut_engine_utils
  add_dependencies(ut_engine ut_engine_utils)
  # Add a test with LD_PRELOAD for ut_engine
  add_test(NAME tests COMMAND ut_engine)
  set_tests_properties(
    tests PROPERTIES ENVIRONMENT "LD_PRELOAD=$<TARGET_FILE:ut_engine_utils>")

  target_link_libraries(
    ut_engine
    PRIVATE enginerpc
            ut_engine_utils
            -Wl,-whole-archive
            cce_core
            log_v2
            opentelemetry
            centagent_lib
            -Wl,-no-whole-archive
            pb_open_telemetry_lib
            centreon_grpc
            centreon_http
            centreon_process
	    Boost::program_options
            pthread
            ${GCOV}
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
            gRPC::grpc++
            crypto
            ssl
            z
            fmt::fmt
            ryml::ryml
            stdc++fs
            dl)

  if(WITH_COVERAGE)
    set(COVERAGE_EXCLUDES
        '${PROJECT_BINARY_DIR}/*' '${PROJECT_SOURCE_DIR}/tests/*'
        '/usr/include/*' '*/.conan/*')
    setup_target_for_coverage(NAME engine-test-coverage EXECUTABLE ut_engine
                              DEPENDENCIES ut_engine)
    set(GCOV gcov)
  endif()
endif()
