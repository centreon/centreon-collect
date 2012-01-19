##
## Copyright 2011-2012 Merethis
##
## This file is part of Centreon Connector Perl.
##
## Centreon Connector Perl is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Connector Perl is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Connector Perl. If not, see
## <http://www.gnu.org/licenses/>.
##

# Global options.
cmake_minimum_required(VERSION 2.8)
project("Centreon Connector Perl" C CXX)
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/connector/perl")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")
include_directories("${PROJECT_SOURCE_DIR}/inc")

# Project version.
set(CONNECTOR_PERL_MAJOR 0)
set(CONNECTOR_PERL_MINOR 1)
set(CONNECTOR_PERL_PATCH 0)
set(CONNECTOR_PERL_VERSION "${CONNECTOR_PERL_MAJOR}.${CONNECTOR_PERL_MINOR}.${CONNECTOR_PERL_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_PERL_VERSION=\"${CONNECTOR_PERL_VERSION}\")

# We will use pkg-config if available.
include(FindPkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(CLIB QUIET "centreon-clib") # Will be used below.
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
  if (CMAKE_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLIB_CFLAGS}")
  else ()
    set(CMAKE_CXX_FLAGS "${CLIB_CFLAGS}")
  endif ()
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

# Find Perl.
include(FindPerlLibs)
if (NOT PERLLIBS_FOUND)
  message(FATAL_ERROR "Could not find Perl libraries.")
endif ()
include_directories("${PERL_INCLUDE_PATH}")

# Perl connector library.
set(CONNECTORLIB "centreonconnectorperl")
add_library("${CONNECTORLIB}" STATIC
  # Sources.
  "${SRC_DIR}/checks/check.cc"
  "${SRC_DIR}/checks/listener.cc"
  "${SRC_DIR}/checks/result.cc"
  "${SRC_DIR}/embedded_perl.cc"
  "${SRC_DIR}/multiplexer.cc"
  "${SRC_DIR}/options.cc"
  "${SRC_DIR}/orders/listener.cc"
  "${SRC_DIR}/orders/parser.cc"
  "${SRC_DIR}/pipe_handle.cc"
  "${SRC_DIR}/policy.cc"
  "${SRC_DIR}/reporter.cc"
  "${SRC_DIR}/script.cc"
  # Headers.
  "${INC_DIR}/checks/check.hh"
  "${INC_DIR}/checks/listener.hh"
  "${INC_DIR}/checks/result.hh"
  "${INC_DIR}/embedded_perl.hh"
  "${INC_DIR}/multiplexer.hh"
  "${INC_DIR}/namespace.hh"
  "${INC_DIR}/options.hh"
  "${INC_DIR}/orders/listener.hh"
  "${INC_DIR}/orders/parser.hh"
  "${INC_DIR}/pipe_handle.hh"
  "${INC_DIR}/policy.hh"
  "${INC_DIR}/reporter.hh"
)
target_link_libraries(
  "${CONNECTORLIB}"
  ${PERL_LIBRARY}
  ${CLIB_LIBRARIES})

# Perl connector.
set(CONNECTOR "centreon_connector_perl")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/main.cc")
target_link_libraries("${CONNECTOR}" ${CONNECTORLIB})

# Installation path.
if (WITH_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${WITH_PREFIX}")
endif ()
if (WITH_PREFIX_BINARY)
  set(PREFIX_BINARY "${WITH_PREFIX_BINARY}")
else ()
  set(PREFIX_BINARY "bin")
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
  set(BUILD_UNIT_TEST "Yes")
  include_directories("${PROJECT_SOURCE_DIR}")

  # Embedded Perl tests.
  #   Simple script execution #1.
  set(TEST_NAME "embedded_perl_run_simple_1")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/embedded_perl/run_simple_1.cc")
  target_link_libraries("${TEST_NAME}" ${CONNECTORLIB})
  add_test("${TEST_NAME}" "${TEST_NAME}")
  #   Simple script execution #2.
  set(TEST_NAME "embedded_perl_run_simple_2")
  add_executable("${TEST_NAME}"
    "${TEST_DIR}/embedded_perl/run_simple_2.cc")
  target_link_libraries("${TEST_NAME}" ${CONNECTORLIB})
  add_test("${TEST_NAME}" "${TEST_NAME}")


  #
  # Process tests.
  #
  # Help.
  add_test("connector_help" "${CONNECTOR}" "--help")
  # Version.
  add_test("connector_version" "${CONNECTOR}" "--version")
  # Invalid flag.
  add_test("connector_invalid_flag" "${CONNECTOR}" "--merethis")
  set_tests_properties("connector_invalid_flag"
    PROPERTIES WILL_FAIL TRUE)

else()
  set(BUILD_UNIT_TEST "No")
endif ()

# Summary.
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  Project")
message(STATUS "    - Name                       Centreon Connector Perl")
message(STATUS "    - Version                    ${CONNECTOR_PERL_VERSION}")
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
message(STATUS "    - Build unit tests           ${BUILD_UNIT_TEST}")
message(STATUS "")
message(STATUS "  Install")
message(STATUS "    - Prefix                     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "    - Binary prefix              ${PREFIX_BINARY}")
message(STATUS "    - Package                    ${PACKAGE_LIST}")
message(STATUS "")
