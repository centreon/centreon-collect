##
## Copyright 2013 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CONAN_INCLUDE_DIRS_JSON11})


# TLS module.
add_library(cbmod SHARED
  # Sources
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/init.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/logger.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/callback.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/callbacks.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/initial.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/internal.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/monitoring_logger.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/neb.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/set_log_data.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_host_execution_time.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_host_latency.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_host_state_change.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_hosts_last.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_service_execution_time.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_service_latency.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_service_state_change.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/active_services_last.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/command_buffers.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/generator.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/hosts.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/hosts_actively_checked.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/hosts_checked.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/hosts_flapping.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/hosts_scheduled.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/passive_host_latency.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/passive_host_state_change.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/passive_hosts_last.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/passive_service_latency.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/passive_service_state_change.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/passive_services_last.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/plugin.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/services.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/services_actively_checked.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/services_checked.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/services_flapping.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/services_scheduled.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/total_host_state_change.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/total_hosts.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/total_service_state_change.cc
  ${CMAKE_SOURCE_DIR}/src/cbmod/statistics/total_services.cc
)

target_link_libraries(cbmod CONAN_PKG::spdlog CONAN_PKG::asio CONAN_PKG::json11 CONAN_PKG::spdlog ccb_neb ccb_core)
set_target_properties(cbmod PROPERTIES PREFIX "")