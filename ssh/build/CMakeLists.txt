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

# Global options.
cmake_minimum_required(VERSION 2.8)
project("Centreon SSH Connector" CXX)
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/connector/ssh")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")
include_directories("${PROJECT_SOURCE_DIR}/inc")

# Project version.
set(CONNECTOR_SSH_MAJOR 20)
set(CONNECTOR_SSH_MINOR 04)
set(CONNECTOR_SSH_PATCH 0)
set(CONNECTOR_SSH_VERSION "${CONNECTOR_SSH_MAJOR}.${CONNECTOR_SSH_MINOR}.${CONNECTOR_SSH_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_SSH_VERSION=\"${CONNECTOR_SSH_VERSION}\")

# We will use pkg-config if available.
include(FindPkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(CLIB QUIET "centreon-clib")  # Will be used below.
  pkg_check_modules(LIBSSH2 QUIET "libssh2")     # Will be used below.
  pkg_check_modules(LIBGCRYPT QUIET "libgcrypt") # Will be used below.
endif ()

# Find libssh2's headers.
if (WITH_LIBSSH2_INCLUDE_DIR)
  find_file(
    LIBSSH2_HEADER_FOUND
    "libssh2.h"
    PATHS "${WITH_LIBSSH2_INCLUDE_DIR}"
    NO_DEFAULT_PATH)
  if (NOT LIBSSH2_HEADER_FOUND)
    message(FATAL_ERROR "Could not find libssh2's headers in ${WITH_LIBSSH2_INCLUDE_DIR}.")
  endif ()
  include_directories("${WITH_LIBSSH2_INCLUDE_DIR}")
elseif (LIBSSH2_FOUND) # Was libssh2 detected with pkg-config ?
  if (CMAKE_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBSSH2_CFLAGS}")
  else ()
    set(CMAKE_CXX_FLAGS "${LIBSSH2_CFLAGS}")
  endif ()
else ()
  find_path(LIBSSH2_INCLUDE_DIR "libssh2.h")
  if (NOT LIBSSH2_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find libssh2's headers (try WITH_LIBSSH2_INCLUDE_DIR).")
  endif ()
  include_directories("${LIBSSH2_INCLUDE_DIR}")
endif ()

# Find libssh2's library.
if (WITH_LIBSSH2_LIBRARIES)
  set(LIBSSH2_LIBRARIES "${WITH_LIBSSH2_LIBRARIES}")
elseif (WITH_LIBSSH2_LIBRARY_DIR)
  find_library(
    LIBSSH2_LIBRARIES
    "ssh2"
    PATHS "${WITH_LIBSSH2_LIBRARY_DIR}"
    NO_DEFAULT_PATH)
  if (NOT LIBSSH2_LIBRARIES)
    message(FATAL_ERROR "Could not find libssh2's library in ${WITH_LIBSSH2_LIBRARY_DIR}.")
  endif ()
elseif (LIBSSH2_FOUND) # Was libssh2 detected with pkg-config ?
  set(LIBSSH2_LIBRARIES "${LIBSSH2_LDFLAGS}")
else ()
  find_library(LIBSSH2_LIBRARIES "ssh2")
  if (NOT LIBSSH2_LIBRARIES)
    message(FATAL_ERROR "Could not find libssh2's library (try WITH_LIBSSH2_LIBRARY_DIR or WITH_LIBSSH2_LIBRARIES).")
  endif ()
endif ()

# Check if libssh2 is using libgcrypt or OpenSSL.
if (WITH_LIBSSH2_WITH_LIBGCRYPT)
  set(LIBSSH2_WITH_LIBGCRYPT "${WITH_LIBSSH2_WITH_LIBGCRYPT}")
else ()
  set(LIBSSH2_WITH_LIBGCRYPT 1)
endif ()
if (LIBSSH2_WITH_LIBGCRYPT)
  # Find libgcrypt's required header.
  if (WITH_LIBGCRYPT_INCLUDE_DIR)
    find_file(
      LIBGCRYPT_HEADER_FOUND
      "gcrypt.h"
      PATHS "${WITH_LIBGCRYPT_INCLUDE_DIR}"
      NO_DEFAULT_PATH)
    if (NOT LIBGCRYPT_HEADER_FOUND)
      message(FATAL_ERROR "Could not find libgcrypt's headers in ${WITH_LIBGCRYPT_INCLUDE_DIR}.")
    endif ()
    include_directories("${WITH_LIBGCRYPT_INCLUDE_DIR}")
  else ()
    find_file(LIBGCRYPT_HEADER_FOUND "gcrypt.h")
    if (NOT LIBGCRYPT_HEADER_FOUND)
      message(FATAL_ERROR "Could not find libgcrypt's headers (try WITH_LIBGCRYPT_INCLUDE_DIR).")
    endif ()
  endif ()

  # Find libgcrypt's library.
  if (WITH_LIBGCRYPT_LIBRARIES)
    set(LIBGCRYPT_LIBRARIES "${WITH_LIBGCRYPT_LIBRARIES}")
  elseif (WITH_LIBGCRYPT_LIBRARY_DIR)
    find_library(
      LIBGCRYPT_LIBRARIES
      "gcrypt"
      PATHS "${WITH_LIBGCRYPT_LIBRARY_DIR}"
      NO_DEFAULT_PATH)
    if (NOT LIBGCRYPT_LIBRARIES)
      message(FATAL_ERROR "Could not find libgcrypt's library in ${WITH_LIBGCRYPT_LIBRARY_DIR}.")
    endif ()
  elseif (LIBGCRYPT_FOUND) # Was libgcrypt detected with pkg-config ?
    set(LIBGCRYPT_LIBRARIES "${LIBGCRYPT_LDFLAGS}")
  else ()
    find_library(LIBGCRYPT_LIBRARIES "gcrypt")
    if (NOT LIBGCRYPT_LIBRARIES)
      message(FATAL_ERROR "Could not find libgcrypt's library (try WITH_LIBGCRYPT_LIBRARY_DIR).")
    endif ()
  endif ()

  # Add macro.
  add_definitions(-DLIBSSH2_WITH_LIBGCRYPT)
endif ()

# Find Centreon Clib's headers.
if (WITH_CENTREON_CLIB_INCLUDE_DIR)
  find_file(
    CLIB_HEADER_FOUND
    "com/centreon/clib/version.hh"
    PATHS "${WITH_CENTREON_CLIB_INCLUDE_DIR}"
    NO_DEFAULT_PATH)
  if (NOT CLIB_HEADER_FOUND)
    message(FATAL_ERROR "Could not find Centreon Clib's headers in ${WITH_CENTREON_CLIB_INCLUDE_DIR}.")
  endif ()
  include_directories("${WITH_CENTREON_CLIB_INCLUDE_DIR}")
  set(CLIB_INCLUDE_DIR "${WITH_CENTREON_CLIB_INCLUDE_DIR}")
elseif (CLIB_FOUND) # Was Centreon Clib detected with pkg-config ?
  if (CMAKE_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLIB_CFLAGS}")
  else ()
    set(CMAKE_CXX_FLAGS "${CLIB_CFLAGS}")
  endif ()
else ()
  find_path(CLIB_INCLUDE_DIR "com/centreon/clib/version.hh" PATH_SUFFIXES "centreon-clib")
  if (NOT CLIB_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find Centreon Clib's headers (try WITH_CENTREON_CLIB_INCLUDE_DIR).")
  endif ()
  include_directories("${CLIB_INCLUDE_DIR}")
endif ()

# Find Centreon Clib's library.
if (WITH_CENTREON_CLIB_LIBRARIES)
  set(CLIB_LIBRARIES "${WITH_CENTREON_CLIB_LIBRARIES}")
elseif (WITH_CENTREON_CLIB_LIBRARY_DIR)
  find_library(
    CLIB_LIBRARIES
    "centreon_clib"
    PATHS "${WITH_CENTREON_CLIB_LIBRARY_DIR}"
    NO_DEFAULT_PATH)
  if (NOT CLIB_LIBRARIES)
    message(FATAL_ERROR "Could not find Centreon Clib's library in ${WITH_CENTREON_CLIB_LIBRARY_DIR}.")
  endif ()
elseif (CLIB_FOUND) # Was Centreon Clib detected with pkg-config ?
  set(CLIB_LIBRARIES "${CLIB_LDFLAGS}")
else ()
  find_library(CLIB_LIBRARIES "centreon_clib")
  if (NOT CLIB_LIBRARIES)
    message(FATAL_ERROR "Could not find Centreon Clib's library (try WITH_CENTREON_CLIB_LIBRARY_DIR or WITH_CENTREON_CLIB_LIBRARIES).")
  endif ()
endif ()

# Should the connector perform check against known_hosts file ?
option(WITH_KNOWN_HOSTS_CHECK "Check hosts against user's known_hosts file." OFF)
if (WITH_KNOWN_HOSTS_CHECK)
  add_definitions(-DWITH_KNOWN_HOSTS_CHECK)
endif ()

# SSH connector library.
set(CONNECTORLIB "centreonconnectorssh")
add_library("${CONNECTORLIB}"
  # Sources.
  "${SRC_DIR}/checks/check.cc"
  "${SRC_DIR}/checks/listener.cc"
  "${SRC_DIR}/checks/result.cc"
  "${SRC_DIR}/checks/timeout.cc"
  "${SRC_DIR}/multiplexer.cc"
  "${SRC_DIR}/options.cc"
  "${SRC_DIR}/orders/listener.cc"
  "${SRC_DIR}/orders/parser.cc"
  "${SRC_DIR}/orders/options.cc"
  "${SRC_DIR}/policy.cc"
  "${SRC_DIR}/reporter.cc"
  "${SRC_DIR}/sessions/credentials.cc"
  "${SRC_DIR}/sessions/listener.cc"
  "${SRC_DIR}/sessions/session.cc"
  "${SRC_DIR}/sessions/socket_handle.cc"
  # Headers.
  "${INC_DIR}/checks/check.hh"
  "${INC_DIR}/checks/listener.hh"
  "${INC_DIR}/checks/result.hh"
  "${INC_DIR}/checks/timeout.hh"
  "${INC_DIR}/multiplexer.hh"
  "${INC_DIR}/namespace.hh"
  "${INC_DIR}/options.hh"
  "${INC_DIR}/orders/listener.hh"
  "${INC_DIR}/orders/parser.hh"
  "${INC_DIR}/orders/options.hh"
  "${INC_DIR}/policy.hh"
  "${INC_DIR}/reporter.hh"
  "${INC_DIR}/sessions/credentials.hh"
  "${INC_DIR}/sessions/listener.hh"
  "${INC_DIR}/sessions/session.hh"
  "${INC_DIR}/sessions/socket_handle.hh"
)
target_link_libraries(
  "${CONNECTORLIB}"
  ${LIBSSH2_LIBRARIES}
  ${CLIB_LIBRARIES}
  ${LIBGCRYPT_LIBRARIES}
  "pthread"
)

# SSH connector.
set(CONNECTOR "centreon_connector_ssh")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/main.cc"
)
target_link_libraries("${CONNECTOR}" ${CONNECTORLIB})

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
install(TARGETS "${CONNECTOR}"
  RUNTIME DESTINATION "${PREFIX_BINARY}")

# Packaging.
include("package.cmake")

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

  # checks namespace tests.
  # result tests.
  #   Default constructor.
  set(TEST_NAME "checks_result_ctor_default")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/ctor_default.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Command ID property.
  set(TEST_NAME "checks_result_command_id")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/command_id.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Error property.
  set(TEST_NAME "checks_result_error")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/error.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Executed
  set(TEST_NAME "checks_result_executed")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/executed.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Exit code property.
  set(TEST_NAME "checks_result_exit_code")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/exit_code.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Output property.
  set(TEST_NAME "checks_result_output")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/output.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Copy constructor.
  set(TEST_NAME "checks_result_ctor_copy")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/ctor_copy.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Assignment operator.
  set(TEST_NAME "checks_result_assignment")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/result/assignment.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # timeout tests.
  #   Constructor.
  set(TEST_NAME "checks_timeout_ctor")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/timeout/ctor.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Copy constructor.
  set(TEST_NAME "checks_timeout_ctor_copy")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/timeout/ctor_copy.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Assignment operator.
  set(TEST_NAME "checks_timeout_assignment")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/timeout/assignment.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Set target check object.
  set(TEST_NAME "checks_timeout_set_check")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/checks/timeout/set_check.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")


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
  # parser tests.
  #   Default constructor.
  set(TEST_NAME "orders_parser_ctor_default")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/ctor_default.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   EOF detection.
  set(TEST_NAME "orders_parser_eof")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/eof.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Close detection.
  set(TEST_NAME "orders_parser_close")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/close.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Error detection.
  set(TEST_NAME "orders_parser_error")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/error.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Version order.
  set(TEST_NAME "orders_parser_version")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/version.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Quit order.
  set(TEST_NAME "orders_parser_quit")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/quit.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Execute order.
  set(TEST_NAME "orders_parser_execute")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/execute.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Order suite.
  set(TEST_NAME "orders_parser_suite")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/suite.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Copy constructor.
  set(TEST_NAME "orders_parser_ctor_copy")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/ctor_copy.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Assignment operator.
  set(TEST_NAME "orders_parser_assignment")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/assignment.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Invalid command.
  set(TEST_NAME "orders_parser_invalid_command")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/invalid_command.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Not enough arguments for execution.
  set(TEST_NAME "orders_parser_execute_not_enough_args")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/execute_not_enough_args.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Invalid command ID in execution order.
  set(TEST_NAME "orders_parser_execute_invalid_id")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/execute_invalid_id.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Invalid timeout in execution order.
  set(TEST_NAME "orders_parser_execute_invalid_timeout")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/execute_invalid_timeout.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Invalid start time in execution order.
  set(TEST_NAME "orders_parser_execute_invalid_start_time")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/orders/parser/execute_invalid_start_time.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")


  #
  # sessions namespace tests.
  #
  # credentials tests.
  #   Default constructor.
  set(TEST_NAME "sessions_credentials_ctor_default")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/ctor_default.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Host property.
  set(TEST_NAME "sessions_credentials_host")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/host.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   User property.
  set(TEST_NAME "sessions_credentials_user")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/user.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Password property.
  set(TEST_NAME "sessions_credentials_password")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/password.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Constructor which set values.
  set(TEST_NAME "sessions_credentials_ctor_values")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/ctor_values.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Copy constructor.
  set(TEST_NAME "sessions_credentials_ctor_copy")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/ctor_copy.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Assignment operator.
  set(TEST_NAME "sessions_credentials_assignment")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/assignment.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Less-than operator.
  set(TEST_NAME "sessions_credentials_less_than")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/sessions/credentials/less_than.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")


  #
  # Root namespace tests.
  #
  # multiplexer tests.
  #   Check singleton.
  set(TEST_NAME "multiplexer_singleton")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/multiplexer/singleton.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # reporter tests.
  #   Default constructor.
  set(TEST_NAME "reporter_ctor_default")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/reporter/ctor_default.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Report protocol version.
  set(TEST_NAME "reporter_send_version")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/reporter/send_version.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Report check result.
  set(TEST_NAME "reporter_send_result")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/reporter/send_result.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Handle has error.
  set(TEST_NAME "reporter_error")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/reporter/error.cc")
  target_link_libraries("${TEST_NAME}" ${ORDERS_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")


  #
  # Process tests.
  #
  set(TEST_LIBRARIES ${CLIB_LIBRARIES})
  # Help.
  add_test("connector_help" "${CONNECTOR}" "--help")
  # Version.
  add_test("connector_version" "${CONNECTOR}" "--version")
  # Invalid flag.
  add_test("connector_invalid_flag" "${CONNECTOR}" "--merethis")
  set_tests_properties("connector_invalid_flag"
    PROPERTIES WILL_FAIL TRUE)
  # Invalid command.
  set(TEST_NAME "connector_invalid_command")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/invalid_command.cc")
  target_link_libraries("${TEST_NAME}" ${CLIB_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # Invalid command ID.
  set(TEST_NAME "connector_invalid_command_id")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/invalid_command_id.cc")
  target_link_libraries("${TEST_NAME}" ${CLIB_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # EOF on stdin.
  set(TEST_NAME "connector_eof_on_stdin")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/eof_on_stdin.cc")
  target_link_libraries("${TEST_NAME}" ${CLIB_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # Quit command.
  set(TEST_NAME "connector_command_quit")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/command_quit.cc")
  target_link_libraries("${TEST_NAME}" ${CLIB_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # Version command.
  set(TEST_NAME "connector_command_version")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/command_version.cc")
  target_link_libraries("${TEST_NAME}" ${CLIB_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # Execute command.
  set(TEST_NAME "connector_command_execute")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/command_execute.cc")
  target_link_libraries("${TEST_NAME}" ${TEST_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # Execute command with a log file
  set(TEST_NAME "connector_command_execute_log_file")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/command_execute_log_file.cc")
  target_link_libraries("${TEST_NAME}" ${TEST_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # Non-existent host.
  set(TEST_NAME "connector_non_existent_host")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/non_existent_host.cc")
  target_link_libraries("${TEST_NAME}" ${TEST_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  # Single check that timeouts.
  set(TEST_NAME "connector_single_check_timeout")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/connector/single_check_timeout.cc")
  target_link_libraries("${TEST_NAME}" ${TEST_LIBRARIES})
  add_test("${TEST_NAME}" "${TEST_NAME}")

endif ()

# Summary.
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  Project")
message(STATUS "    - Name                       Centreon SSH Connector")
message(STATUS "    - Version                    ${CONNECTOR_SSH_VERSION}")
if (WITH_KNOWN_HOSTS_CHECK)
  message(STATUS "    - Known hosts check          enabled")
else ()
  message(STATUS "    - Known hosts check          disabled")
endif ()
message(STATUS "")
message(STATUS "  System")
message(STATUS "    - Name                       ${CMAKE_SYSTEM_NAME}")
message(STATUS "    - Version                    ${CMAKE_SYSTEM_VERSION}")
message(STATUS "    - Processor                  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  Build")
message(STATUS "    - Compiler                   ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "    - Extra compilation flags    ${CMAKE_CXX_FLAGS}")
if (WITH_TESTING)
  message(STATUS "    - Unit tests                 enabled")
else ()
  message(STATUS "    - Unit tests                 disabled")
endif ()
message(STATUS "")
message(STATUS "  Install")
message(STATUS "    - Prefix                     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "    - Binary prefix              ${PREFIX_BINARY}")
message(STATUS "    - Package                    ${PACKAGE_LIST}")
message(STATUS "")
message(STATUS "  Libraries")
message(STATUS "    - clib include directory     ${CLIB_INCLUDE_DIR}")
message(STATUS "    - clib library directory     ${CLIB_LIBRARIES}")
