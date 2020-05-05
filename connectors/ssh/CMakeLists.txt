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

# Should the connector perform check against known_hosts file ?
option(WITH_KNOWN_HOSTS_CHECK "Check hosts against user's known_hosts file." OFF)
if (WITH_KNOWN_HOSTS_CHECK)
  add_definitions(-DWITH_KNOWN_HOSTS_CHECK)
endif ()

include_directories(${CMAKE_SOURCE_DIR}/connectors/ssh/inc)
# SSH connector.
add_executable(centreon_connector_ssh
  # Sources.
  ${CMAKE_SOURCE_DIR}/connectors/common/src/log.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/main.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/checks/check.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/checks/result.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/checks/timeout.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/multiplexer.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/options.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/orders/parser.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/orders/options.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/policy.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/reporter.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/sessions/credentials.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/sessions/session.cc
  ${CMAKE_SOURCE_DIR}/connectors/ssh/src/sessions/socket_handle.cc
  # Headers.
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/checks/check.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/checks/listener.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/checks/result.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/checks/timeout.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/multiplexer.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/namespace.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/options.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/orders/listener.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/orders/parser.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/orders/options.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/policy.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/reporter.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/sessions/credentials.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/sessions/listener.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/sessions/session.hh
  ${CMAKE_SOURCE_DIR}/connectors/ssh/inc/com/centreon/connector/ssh/sessions/socket_handle.hh
)
target_link_libraries(centreon_connector_ssh ${LIBSSH2_LIBRARIES}
   ${LIBGCRYPT_LIBRARIES} centreon-clib CONAN_PKG::spdlog pthread)

# Installation path.
if (WITH_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${WITH_PREFIX}")
endif ()
if (WITH_PREFIX_BINARY)
  set(PREFIX_BINARY "${WITH_PREFIX_BINARY}")
else ()
  set(PREFIX_BINARY "${CMAKE_INSTALL_PREFIX}/bin")
endif ()

# Install rules.
install(TARGETS centreon_connector_ssh RUNTIME DESTINATION ${PREFIX_BINARY})

# Code coverage on unit tests
option(WITH_COVERAGE "Add code coverage on unit tests." OFF)
if (WITH_COVERAGE)
  set(WITH_TESTING ON)
  set(CMAKE_BUILD_TYPE "Debug")
  include(CodeCoverage.cmake)
  APPEND_COVERAGE_COMPILER_FLAGS()
endif ()