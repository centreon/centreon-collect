##
## Copyright 2011-2015,2017-2019 Centreon
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

# SSH connector.
add_executable(centreon_connector_ssh
  # Sources.
  ${CMAKE_SOURCE_DIR}/src/connectors/common/log.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/main.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/checks/check.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/checks/result.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/checks/timeout.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/multiplexer.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/options.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/orders/parser.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/orders/options.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/policy.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/reporter.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/sessions/credentials.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/sessions/session.cc
  ${CMAKE_SOURCE_DIR}/src/connectors/ssh/sessions/socket_handle.cc
)
target_link_libraries(centreon_connector_ssh CONAN_PKG::libssh2 centreon-clib CONAN_PKG::spdlog pthread)
