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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

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
  "${SRC_DIR}/checks/result.cc"
  "${SRC_DIR}/checks/timeout.cc"
  "${SRC_DIR}/multiplexer.cc"
  "${SRC_DIR}/options.cc"
  "${SRC_DIR}/orders/parser.cc"
  "${SRC_DIR}/orders/options.cc"
  "${SRC_DIR}/policy.cc"
  "${SRC_DIR}/reporter.cc"
  "${SRC_DIR}/sessions/credentials.cc"
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
