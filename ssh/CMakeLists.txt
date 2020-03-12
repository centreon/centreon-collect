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

include(${CMAKE_SOURCE_DIR}/cmake/Findclib.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindSSH.cmake)
# Project version.
set(CONNECTOR_SSH_MAJOR 20)
set(CONNECTOR_SSH_MINOR 04)
set(CONNECTOR_SSH_PATCH 0)
set(CONNECTOR_SSH_VERSION "${CONNECTOR_SSH_MAJOR}.${CONNECTOR_SSH_MINOR}.${CONNECTOR_SSH_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_SSH_VERSION=\"${CONNECTOR_SSH_VERSION}\")


# Should the connector perform check against known_hosts file ?
option(WITH_KNOWN_HOSTS_CHECK "Check hosts against user's known_hosts file." OFF)
if (WITH_KNOWN_HOSTS_CHECK)
  add_definitions(-DWITH_KNOWN_HOSTS_CHECK)
endif ()

include_directories(${CMAKE_SOURCE_DIR}/ssh/inc)
# SSH connector.
add_executable(centreon_connector_ssh
  # Sources.
  ${CMAKE_SOURCE_DIR}/ssh/src/main.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/checks/check.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/checks/result.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/checks/timeout.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/multiplexer.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/options.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/orders/parser.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/orders/options.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/policy.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/reporter.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/sessions/credentials.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/sessions/session.cc
  ${CMAKE_SOURCE_DIR}/ssh/src/sessions/socket_handle.cc
  # Headers.
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/checks/check.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/checks/listener.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/checks/result.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/checks/timeout.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/multiplexer.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/namespace.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/options.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/orders/listener.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/orders/parser.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/orders/options.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/policy.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/reporter.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/sessions/credentials.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/sessions/listener.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/sessions/session.hh
  ${CMAKE_SOURCE_DIR}/ssh/inc/com/centreon/connector/ssh/sessions/socket_handle.hh
)
target_link_libraries(centreon_connector_ssh ${LIBSSH2_LIBRARIES}
  ${CLIB_LIBRARIES} ${LIBGCRYPT_LIBRARIES} pthread)

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

# Testing.
option(WITH_TESTING "Build unit tests." OFF)
if (WITH_TESTING)
  # Enable testing.
  enable_testing()
  include_directories("${PROJECT_SOURCE_DIR}")
  get_property(CONNECTOR_SSH_BINARY
    TARGET "${CONNECTOR}"
    PROPERTY LOCATION)
  configure_file(
    "${TEST_DIR}/connector/binary.hh.in"
    "${TEST_DIR}/connector/binary.hh")

set(CONNECTOR_TEST_LIST
    ${TEST_DIR}/checks/result/ctor_default.cc
    ${TEST_DIR}/checks/result/command_id.cc
    ${TEST_DIR}/checks/result/error.cc
    ${TEST_DIR}/checks/result/executed.cc
    ${TEST_DIR}/checks/result/exit_code.cc
    ${TEST_DIR}/checks/result/output.cc
    ${TEST_DIR}/checks/result/ctor_copy.cc
    ${TEST_DIR}/checks/result/assignment.cc
    ${TEST_DIR}/checks/timeout/ctor.cc
    ${TEST_DIR}/checks/timeout/assignment.cc
    ${TEST_DIR}/sessions/credentials/ctor_default.cc
    ${TEST_DIR}/sessions/credentials/host.cc
    ${TEST_DIR}/sessions/credentials/user.cc
    ${TEST_DIR}/sessions/credentials/password.cc
    ${TEST_DIR}/sessions/credentials/ctor_values.cc
    ${TEST_DIR}/sessions/credentials/ctor_copy.cc
    ${TEST_DIR}/sessions/credentials/assignment.cc
    ${TEST_DIR}/sessions/credentials/less_than.cc
    ${TEST_DIR}/reporter/ctor_default.cc
   )

  foreach (name ${CONNECTOR_TEST_LIST})
    # name cc file => bin_name binary name
    string(REGEX REPLACE "^${TEST_DIR}/" "" bin_name ${name})
    string(REGEX REPLACE "/" "_" bin_name ${bin_name})
    string(REGEX REPLACE ".cc$" "" bin_name ${bin_name})

    add_executable("${bin_name}" "${name}")
    target_link_libraries("${bin_name}" "${CONNECTORLIB}")
    add_test("${bin_name}" "${bin_name}")
    list(APPEND TEST_BIN_LIST ${bin_name})
  endforeach ()

  #
  # orders namespace tests.
  #
  # Common library.
  add_library("test_orders" STATIC
    # Sources.
    "${TEST_DIR}/orders/buffer_handle.cc"
    "${TEST_DIR}/orders/fake_listener.cc"
    # Headers.
    "${TEST_DIR}/orders/buffer_handle.hh"
    "${TEST_DIR}/orders/fake_listener.hh")
  set(ORDERS_LIBRARIES "test_orders" "${CONNECTORLIB}")

  set(ORDERS_TEST_LIST
    ${TEST_DIR}/reporter/send_version.cc
    ${TEST_DIR}/reporter/send_result.cc
    ${TEST_DIR}/reporter/error.cc
    ${TEST_DIR}/orders/parser/execute.cc
    ${TEST_DIR}/orders/parser/close.cc
    ${TEST_DIR}/orders/parser/ctor_default.cc
    ${TEST_DIR}/orders/parser/eof.cc
    ${TEST_DIR}/orders/parser/error.cc
    ${TEST_DIR}/orders/parser/execute_invalid_id.cc
    ${TEST_DIR}/orders/parser/execute_invalid_start_time.cc
    ${TEST_DIR}/orders/parser/execute_invalid_timeout.cc
    ${TEST_DIR}/orders/parser/execute_not_enough_args.cc
    ${TEST_DIR}/orders/parser/invalid_command.cc
    ${TEST_DIR}/orders/parser/quit.cc
    ${TEST_DIR}/orders/parser/suite.cc
    ${TEST_DIR}/orders/parser/version.cc
    )

  foreach (name ${ORDERS_TEST_LIST})
    # name cc file => bin_name binary name
    string(REGEX REPLACE "^${TEST_DIR}/" "" bin_name ${name})
    string(REGEX REPLACE "/" "_" bin_name ${bin_name})
    string(REGEX REPLACE ".cc$" "" bin_name ${bin_name})

    add_executable("${bin_name}" "${name}")
    target_link_libraries("${bin_name}" "${ORDERS_LIBRARIES}")
    add_test("${bin_name}" "${bin_name}")
    list(APPEND TEST_BIN_LIST ${bin_name})
  endforeach ()

  #
  # Process tests.
  #
  set(TEST_LIBRARIES ${CLIB_LIBRARIES} pthread)
  # Help.
  add_test("connector_help" "${CONNECTOR}" "--help")
  # Version.
  add_test("connector_version" "${CONNECTOR}" "--version")
  # Invalid flag.
  add_test("connector_invalid_flag" "${CONNECTOR}" "--merethis")
  set_tests_properties("connector_invalid_flag"
    PROPERTIES WILL_FAIL TRUE)

  set(CLIB_TEST_LIST
    ${TEST_DIR}/connector/invalid_command.cc
    ${TEST_DIR}/connector/invalid_command_id.cc
    ${TEST_DIR}/connector/eof_on_stdin.cc
    ${TEST_DIR}/connector/command_quit.cc
    ${TEST_DIR}/connector/command_version.cc
    )

  foreach (name ${CLIB_TEST_LIST})
    # name cc file => bin_name binary name
    string(REGEX REPLACE "^${TEST_DIR}/" "" bin_name ${name})
    string(REGEX REPLACE "/" "_" bin_name ${bin_name})
    string(REGEX REPLACE ".cc$" "" bin_name ${bin_name})

    add_executable("${bin_name}" "${name}")
    target_link_libraries("${bin_name}" "${CLIB_LIBRARIES}")
    add_test("${bin_name}" "${bin_name}")
    list(APPEND TEST_BIN_LIST ${bin_name})
  endforeach ()

  set(TEST_LIST
    ${TEST_DIR}/connector/command_execute.cc
    ${TEST_DIR}/connector/command_execute_log_file.cc
    ${TEST_DIR}/connector/non_existent_host.cc
    ${TEST_DIR}/connector/single_check_timeout.cc
    )

  foreach (name ${TEST_LIST})
    # name cc file => bin_name binary name
    string(REGEX REPLACE "^${TEST_DIR}/" "" bin_name ${name})
    string(REGEX REPLACE "/" "_" bin_name ${bin_name})
    string(REGEX REPLACE ".cc$" "" bin_name ${bin_name})

    add_executable("${bin_name}" "${name}")
    target_link_libraries("${bin_name}" "${TEST_LIBRARIES}")
    add_test("${bin_name}" "${bin_name}")
    list(APPEND TEST_BIN_LIST ${bin_name})
  endforeach ()

  if (WITH_COVERAGE)
    set(COVERAGE_EXCLUDES '${PROJECT_BINARY_DIR}/*' '${PROJECT_SOURCE_DIR}/test/*' '/usr/include/*')
    SETUP_TARGET_FOR_COVERAGE(
      NAME test-coverage
      EXECUTABLE ctest
      DEPENDENCIES
      ${TEST_BIN_LIST}
      )
  endif ()
endif ()
