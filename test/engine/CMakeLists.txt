##
## Copyright 2016 Centreon
##
## This file is part of Centreon Engine.
##
## Centreon Engine is free software: you can redistribute it and/or
## modify it under the terms of the GNU General Public License version 2
## as published by the Free Software Foundation.
##
## Centreon Engine is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Centreon Engine. If not, see
## <http://www.gnu.org/licenses/>.
##

# Set directory.
add_definitions(-DENGINERPC_TESTS_PATH="${CMAKE_SOURCE_DIR}/test/enginerpc")

add_executable(engine_rpc_client ${CMAKE_SOURCE_DIR}/test/engine/enginerpc/client.cc)
target_link_libraries(engine_rpc_client cce_rpc CONAN_PKG::grpc)

include_directories(${CMAKE_SOURCE_DIR}/test/engine)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src/centengine)
include_directories(${CMAKE_BINARY_DIR})

add_executable("bin_connector_test_run"
  ${CMAKE_SOURCE_DIR}/test/engine/commands/bin_connector_test_run.cc)
target_link_libraries(bin_connector_test_run cce_core pthread)

add_executable(cce_ut
  # Sources.
  ${CMAKE_SOURCE_DIR}/src/external_command/commands.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/internal.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/processing.cc
  ${CMAKE_SOURCE_DIR}/src/centengine/engine_impl.cc
  ${CMAKE_SOURCE_DIR}/src/centengine/enginerpc.cc
  ${CMAKE_SOURCE_DIR}/test/engine/parse-check-output.cc
  ${CMAKE_SOURCE_DIR}/test/engine/checks/service_check.cc
  ${CMAKE_SOURCE_DIR}/test/engine/checks/service_retention.cc
  ${CMAKE_SOURCE_DIR}/test/engine/checks/anomalydetection.cc
  ${CMAKE_SOURCE_DIR}/test/engine/commands/simple-command.cc
  ${CMAKE_SOURCE_DIR}/test/engine/commands/connector.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-anomalydetection.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-command.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-connector.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-contact.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-contactgroup.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-global.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-host.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-hostescalation.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-hostdependency.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-hostgroup.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-service.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-serviceescalation.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/applier/applier-servicegroup.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/contact.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/host.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/object.cc
  ${CMAKE_SOURCE_DIR}/test/engine/configuration/service.cc
  ${CMAKE_SOURCE_DIR}/test/engine/contacts/contactgroup-config.cc
  ${CMAKE_SOURCE_DIR}/test/engine/contacts/simple-contactgroup.cc
  ${CMAKE_SOURCE_DIR}/test/engine/custom_vars/extcmd.cc
  ${CMAKE_SOURCE_DIR}/test/engine/downtimes/downtime.cc
  ${CMAKE_SOURCE_DIR}/test/engine/downtimes/downtime_finder.cc
  ${CMAKE_SOURCE_DIR}/test/engine/enginerpc/enginerpc.cc
  ${CMAKE_SOURCE_DIR}/test/engine/helper.cc
  ${CMAKE_SOURCE_DIR}/test/engine/macros/macro.cc
  ${CMAKE_SOURCE_DIR}/test/engine/macros/url_encode.cc
  ${CMAKE_SOURCE_DIR}/test/engine/external_commands/anomalydetection.cc
  ${CMAKE_SOURCE_DIR}/test/engine/external_commands/host.cc
  ${CMAKE_SOURCE_DIR}/test/engine/external_commands/service.cc
  ${CMAKE_SOURCE_DIR}/test/engine/main.cc
  ${CMAKE_SOURCE_DIR}/test/engine/loop/loop.cc
  ${CMAKE_SOURCE_DIR}/test/engine/notifications/host_downtime_notification.cc
  ${CMAKE_SOURCE_DIR}/test/engine/notifications/host_flapping_notification.cc
  ${CMAKE_SOURCE_DIR}/test/engine/notifications/host_normal_notification.cc
  ${CMAKE_SOURCE_DIR}/test/engine/notifications/host_recovery_notification.cc
  ${CMAKE_SOURCE_DIR}/test/engine/notifications/service_normal_notification.cc
  ${CMAKE_SOURCE_DIR}/test/engine/notifications/service_flapping_notification.cc
  ${CMAKE_SOURCE_DIR}/test/engine/perfdata/perfdata.cc
  ${CMAKE_SOURCE_DIR}/test/engine/retention/host.cc
  ${CMAKE_SOURCE_DIR}/test/engine/retention/service.cc
  ${CMAKE_SOURCE_DIR}/test/engine/string/string.cc
  ${CMAKE_SOURCE_DIR}/test/engine/test_engine.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/between_two_years.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/calendar_date.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/dst_backward.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/dst_forward.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/earliest_daterange_first.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/exclusion.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/generic_month_date.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/normal_weekday.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/offset_weekday_of_generic_month.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/offset_weekday_of_specific_month.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/precedence.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/skip_interval.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/get_next_valid_time/specific_month_date.cc
  ${CMAKE_SOURCE_DIR}/test/engine/timeperiod/utils.cc
  )

target_link_libraries(cce_ut cce_rpc cce_core CONAN_PKG::gtest CONAN_PKG::grpc)
