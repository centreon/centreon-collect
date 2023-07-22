#
# Copyright 2016, 2020-2023 Centreon
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
  include_directories(${PROJECT_SOURCE_DIR} ${MODULE_DIR}/inc)

  # Set directory.
  set(TESTS_DIR "${PROJECT_SOURCE_DIR}/tests")
  include_directories(${PROJECT_SOURCE_DIR}/enginerpc)
  add_definitions("-DENGINERPC_TESTS_PATH=${TESTS_DIR}/enginerpc"
                  "-DENGINE_CFG_TEST=\"${TESTS_DIR}/cfg_files\"")

  add_executable(rpc_client_engine ${TESTS_DIR}/enginerpc/client.cc)

  target_link_libraries(
    rpc_client_engine
    cerpc
    centreon_common
    CONAN_PKG::grpc
    CONAN_PKG::openssl
    CONAN_PKG::zlib
    dl
    pthread)

  add_executable(bin_connector_test_run
                 "${TESTS_DIR}/commands/bin_connector_test_run.cc")
  target_link_libraries(bin_connector_test_run cce_core pthread)
  target_precompile_headers(bin_connector_test_run REUSE_FROM cce_core)

  set(ut_sources
      # Sources.
      "${TESTS_DIR}/checks/anomalydetection.cc"
      "${TESTS_DIR}/checks/pb_service_check.cc"
      "${TESTS_DIR}/checks/pb_service_retention.cc"
      "${TESTS_DIR}/checks/service_check.cc"
      "${TESTS_DIR}/checks/service_retention.cc"
      "${TESTS_DIR}/commands/connector.cc"
      "${TESTS_DIR}/commands/environment.cc"
      "${TESTS_DIR}/commands/pbsimple-command.cc"
      "${TESTS_DIR}/commands/simple-command.cc"
      "${TESTS_DIR}/configuration/applier/applier-anomalydetection.cc"
      "${TESTS_DIR}/configuration/applier/applier-command.cc"
      "${TESTS_DIR}/configuration/applier/applier-connector.cc"
      "${TESTS_DIR}/configuration/applier/applier-contact.cc"
      "${TESTS_DIR}/configuration/applier/applier-contactgroup.cc"
      "${TESTS_DIR}/configuration/applier/applier-global.cc"
      "${TESTS_DIR}/configuration/applier/applier-host.cc"
      "${TESTS_DIR}/configuration/applier/applier-hostdependency.cc"
      "${TESTS_DIR}/configuration/applier/applier-hostescalation.cc"
      "${TESTS_DIR}/configuration/applier/applier-hostgroup.cc"
      "${TESTS_DIR}/configuration/applier/applier-log.cc"
      "${TESTS_DIR}/configuration/applier/applier-pbanomalydetection.cc"
      "${TESTS_DIR}/configuration/applier/applier-pbcommand.cc"
      "${TESTS_DIR}/configuration/applier/applier-pbconnector.cc"
      "${TESTS_DIR}/configuration/applier/applier-pbcontact.cc"
      "${TESTS_DIR}/configuration/applier/applier-pbcontactgroup.cc"
      "${TESTS_DIR}/configuration/applier/applier-service.cc"
      "${TESTS_DIR}/configuration/applier/applier-servicedependency.cc"
      "${TESTS_DIR}/configuration/applier/applier-serviceescalation.cc"
      "${TESTS_DIR}/configuration/applier/applier-servicegroup.cc"
      "${TESTS_DIR}/configuration/applier/applier-severity.cc"
      "${TESTS_DIR}/configuration/applier/applier-state.cc"
      "${TESTS_DIR}/configuration/applier/applier-tag.cc"
      "${TESTS_DIR}/configuration/applier/applier-timeperiod.cc"
      "${TESTS_DIR}/configuration/contact.cc"
      "${TESTS_DIR}/configuration/host.cc"
      "${TESTS_DIR}/configuration/object.cc"
      "${TESTS_DIR}/configuration/service.cc"
      "${TESTS_DIR}/configuration/severity.cc"
      "${TESTS_DIR}/configuration/tag.cc"
      "${TESTS_DIR}/configuration/timeperiod-test.cc"
      "${TESTS_DIR}/contacts/contactgroup-config.cc"
      "${TESTS_DIR}/contacts/simple-contactgroup.cc"
      "${TESTS_DIR}/custom_vars/extcmd.cc"
      "${TESTS_DIR}/downtimes/downtime.cc"
      "${TESTS_DIR}/downtimes/downtime_finder.cc"
      "${TESTS_DIR}/enginerpc/enginerpc.cc"
      "${TESTS_DIR}/external_commands/anomalydetection.cc"
      "${TESTS_DIR}/external_commands/host.cc"
      "${TESTS_DIR}/external_commands/service.cc"
      "${TESTS_DIR}/helper.cc"
      "${TESTS_DIR}/loop/loop.cc"
      "${TESTS_DIR}/macros/macro.cc"
      "${TESTS_DIR}/macros/macro_hostname.cc"
      "${TESTS_DIR}/macros/macro_service.cc"
      "${TESTS_DIR}/main.cc"
      "${TESTS_DIR}/notifications/host_downtime_notification.cc"
      "${TESTS_DIR}/notifications/host_flapping_notification.cc"
      "${TESTS_DIR}/notifications/host_normal_notification.cc"
      "${TESTS_DIR}/notifications/host_recovery_notification.cc"
      "${TESTS_DIR}/notifications/service_downtime_notification_test.cc"
      "${TESTS_DIR}/notifications/service_flapping_notification.cc"
      "${TESTS_DIR}/notifications/service_normal_notification.cc"
      "${TESTS_DIR}/notifications/service_timeperiod_notification.cc"
      "${TESTS_DIR}/parse-check-output.cc"
      "${TESTS_DIR}/perfdata/perfdata.cc"
      "${TESTS_DIR}/retention/host.cc"
      "${TESTS_DIR}/retention/service.cc"
      "${TESTS_DIR}/string/string.cc"
      "${TESTS_DIR}/test_engine.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/between_two_years.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/calendar_date.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/dst_backward.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/dst_forward.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/earliest_daterange_first.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/exceptions_test.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/exclusion.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/generic_month_date.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/normal_weekday.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/offset_weekday_of_generic_month.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/offset_weekday_of_specific_month.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/precedence.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/skip_interval.cc"
      "${TESTS_DIR}/timeperiod/get_next_valid_time/specific_month_date.cc"
      "${TESTS_DIR}/timeperiod/utils.cc"

      # Headers.
      "${TESTS_DIR}/test_engine.hh"
      "${TESTS_DIR}/test_engine.hh"
      "${TESTS_DIR}/timeperiod/utils.hh"
      "${TESTS_DIR}/timeperiod/utils.hh")

  # Unit test executable.
  include_directories(${TESTS_DIR})

  if(WITH_ASAN)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG
        "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address"
    )
  endif()

  # utils.cc can't be compiled with precomp headers so it's compiled apart
  add_library(ut_engine_utils STATIC "${TESTS_DIR}/timeperiod/utils.cc")

  add_executable(ut_engine ${ut_sources})
  target_precompile_headers(ut_engine REUSE_FROM cce_core)

  set_target_properties(
    ut_engine rpc_client_engine bin_connector_test_run
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/tests
               RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/tests)

  # file used by timeperiod-test.cc
  file(COPY ${TESTS_DIR}/configuration/timeperiods.cfg
       DESTINATION ${CMAKE_BINARY_DIR}/tests/)

  add_test(NAME tests COMMAND ut_engine)

  if(WITH_COVERAGE)
    set(COVERAGE_EXCLUDES
        '${PROJECT_BINARY_DIR}/*' '${PROJECT_SOURCE_DIR}/tests/*'
        '/usr/include/*' '*/.conan/*')
    setup_target_for_coverage(NAME engine-test-coverage EXECUTABLE ut_engine
                              DEPENDENCIES ut_engine)
    set(GCOV gcov)
  endif()

  target_link_libraries(
    ut_engine
    ${ENGINERPC}
    ut_engine_utils
    cce_core
    enginelog
    pthread
    ${GCOV}
    CONAN_PKG::gtest
    CONAN_PKG::grpc
    CONAN_PKG::openssl
    CONAN_PKG::zlib
    CONAN_PKG::fmt
    dl)
endif()
