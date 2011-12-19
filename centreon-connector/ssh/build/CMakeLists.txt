##
## Copyright 2011 Merethis
##
## This file is part of Centreon Connector SSH.
##
## Centreon Connector SSH is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Connector SSH is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Connector SSH. If not, see
## <http://www.gnu.org/licenses/>.
##

# Global options.
cmake_minimum_required(VERSION 2.8)
project("Centreon Connector SSH" CXX)
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/connector/ssh")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")
include_directories("${PROJECT_SOURCE_DIR}/inc")

# We will use pkg-config if available.
include(FindPkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(CLIB QUIET "centreon-clib") # Will be used below.
  pkg_check_modules(LIBSSH2 QUIET "libssh2")    # Will be used below.
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
  set_property(GLOBAL APPEND PROPERTY CMAKE_CXX_FLAGS "${LIBSSH2_CFLAGS}")
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
elseif (CLIB_FOUND) # Was Centreon Clib detected with pkg-config ?
  set_property(GLOBAL APPEND PROPERTY CMAKE_CXX_FLAGS "${CLIB_CFLAGS}")
else ()
  find_path(CLIB_INCLUDE_DIR "com/centreon/clib/version.hh")
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

# SSH connector library.
set(CONNECTORLIB "centreonconnectorssh")
add_library("${CONNECTORLIB}"
  # Sources.
#   "${SRC_DIR}/channel.cc"
#   "${SRC_DIR}/credentials.cc"
  "${SRC_DIR}/multiplexer.cc"
#   "${SRC_DIR}/session.cc"
#   "${SRC_DIR}/sessions.cc"
#   "${SRC_DIR}/std_io.cc"
  # Headers.
#   "${INC_DIR}/channel.hh"
#   "${INC_DIR}/credentials.hh"
  "${INC_DIR}/multiplexer.hh"
  "${INC_DIR}/namespace.hh"
#   "${INC_DIR}/session.hh"
#   "${INC_DIR}/sessions.hh"
#   "${INC_DIR}/std_io.hh"
)
target_link_libraries(
  "${CONNECTORLIB}"
  ${LIBSSH2_LIBRARIES}
  ${CLIB_LIBRARIES})

# SSH connector.
set(CONNECTOR "centreon_connector_ssh")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/main.cc"
)
target_link_libraries("${CONNECTOR}" ${CONNECTORLIB})

# Installation.
# XXX

# Packages.
# XXX

# Testing.
option(WITH_TESTING "Build unit tests." OFF)
if (WITH_TESTING)
  # Enable testing.
  enable_testing()

  # multiplexer tests.
  #   Check singleton.
  set(TEST_NAME "multiplexer_singleton")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/multiplexer/singleton.cc")
  target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
  add_test("${TEST_NAME}" "${TEST_NAME}")

endif ()
