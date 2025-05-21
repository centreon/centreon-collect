#
# Copyright 2011-2015,2017-2019 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

# Should the connector perform check against known_hosts file ?
option(WITH_KNOWN_HOSTS_CHECK "Check hosts against user's known_hosts file." OFF)

if(WITH_KNOWN_HOSTS_CHECK)
  add_definitions(-DWITH_KNOWN_HOSTS_CHECK)
endif()

include_directories(${PROJECT_SOURCE_DIR}/ssh/inc ${Boost_INCLUDE_DIRS}
                    ${Libssh2_INCLUDE_DIR})
link_directories(${CMAKE_SOURCE_DIR}/build/centreon-clib/)

# SSH connector.
add_executable(
  centreon_connector_ssh
  # Sources.
  ${PROJECT_SOURCE_DIR}/common/src/log.cc
  ${PROJECT_SOURCE_DIR}/common/src/parser.cc
  ${PROJECT_SOURCE_DIR}/common/src/reporter.cc
  ${PROJECT_SOURCE_DIR}/common/src/result.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/main.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/checks/check.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/options.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/orders/parser.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/orders/options.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/policy.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/sessions/credentials.cc
  ${PROJECT_SOURCE_DIR}/ssh/src/sessions/session.cc
  # Headers.
  ${PROJECT_SOURCE_DIR}/common/inc/com/centreon/connector/parser.hh
  ${PROJECT_SOURCE_DIR}/common/inc/com/centreon/connector/reporter.hh
  ${PROJECT_SOURCE_DIR}/common/inc/com/centreon/connector/result.hh
  ${PROJECT_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/checks/check.hh
  ${PROJECT_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/options.hh
  ${PROJECT_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/orders/options.hh
  ${PROJECT_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/policy.hh
  ${PROJECT_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/sessions/credentials.hh
  ${PROJECT_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/sessions/session.hh)
add_dependencies(centreon_connector_ssh centreon_clib)
target_link_libraries(
  centreon_connector_ssh
  PRIVATE
  centreon_clib
  spdlog::spdlog
  fmt::fmt
  ssh2
  absl::any absl::log absl::base absl::bits
  absl::raw_hash_set absl::hash absl::low_level_hash absl::hashtablez_sampler
  pthread)

target_precompile_headers(centreon_connector_ssh REUSE_FROM
                          centreon_connector_perl)

# Install rules.
install(TARGETS centreon_connector_ssh RUNTIME DESTINATION ${PREFIX_CONNECTORS})
