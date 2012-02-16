##
## Copyright 2011-2012 Merethis
##
## This file is part of Centreon Connector ICMP.
##
## Centreon Connector ICMP is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Connector ICMP is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Connector ICMP. If not, see
## <http://www.gnu.org/licenses/>.
##

# Global options.
cmake_minimum_required(VERSION 2.8)
project("Centreon Connector ICMP" C CXX)
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/connector/icmp")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/inc")

# Project version.
set(CONNECTOR_ICMP_MAJOR 0)
set(CONNECTOR_ICMP_MINOR 1)
set(CONNECTOR_ICMP_PATCH 0)
set(CONNECTOR_ICMP_VERSION "${CONNECTOR_ICMP_MAJOR}.${CONNECTOR_ICMP_MINOR}.${CONNECTOR_ICMP_PATCH}")

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

# Generate version header.
configure_file(
  "${INC_DIR}/version.hh.in"
  "${INC_DIR}/version.hh"
)

# ICMP connector library.
set(CONNECTORLIB "centreonconnectoricmp")
add_library("${CONNECTORLIB}"
  # Sources.
  "${SRC_DIR}/check.cc"
  "${SRC_DIR}/check_dispatch.cc"
  "${SRC_DIR}/check_observer.cc"
  "${SRC_DIR}/cmd_dispatch.cc"
  "${SRC_DIR}/cmd_execute.cc"
  "${SRC_DIR}/cmd_options.cc"
  "${SRC_DIR}/host.cc"
  "${SRC_DIR}/icmp_info.cc"
  "${SRC_DIR}/icmp_socket.cc"
  "${SRC_DIR}/interrupt.cc"
  "${SRC_DIR}/packet.cc"
  "${SRC_DIR}/packet_dispatch.cc"
  "${SRC_DIR}/packet_observer.cc"
  "${SRC_DIR}/request.cc"
  "${SRC_DIR}/result.cc"
  "${SRC_DIR}/version.cc"
  # Headers.
  "${INC_DIR}/check.hh"
  "${INC_DIR}/check_dispatch.hh"
  "${INC_DIR}/check_observer.hh"
  "${INC_DIR}/cmd_dispatch.hh"
  "${INC_DIR}/cmd_execute.hh"
  "${INC_DIR}/cmd_options.hh"
  "${INC_DIR}/host.hh"
  "${INC_DIR}/icmp_info.hh"
  "${INC_DIR}/icmp_socket.hh"
  "${INC_DIR}/interrupt.hh"
  "${INC_DIR}/namespace.hh"
  "${INC_DIR}/packet.hh"
  "${INC_DIR}/packet_dispatch.hh"
  "${INC_DIR}/packet_observer.hh"
  "${INC_DIR}/request.hh"
  "${INC_DIR}/result.hh"
  "${INC_DIR}/version.hh"
)
target_link_libraries(
  "${CONNECTORLIB}"
  ${CLIB_LIBRARIES}
)

# ICMP connector.
set(CONNECTOR "centreon_connector_icmp")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/main.cc"
)
target_link_libraries(
  "${CONNECTOR}"
  ${CONNECTORLIB}
)

# Add subdirectories.
if (WITH_TESTING)
  include(CTest)
  add_subdirectory("test")
endif ()

# Installation path.
if (WITH_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${WITH_PREFIX}")
endif ()
if (WITH_PREFIX_BINARY)
  set(PREFIX_BINARY "${WITH_PREFIX_BINARY}")
else ()
  set(PREFIX_BINARY "bin")
endif ()

# Install project.
install(
  TARGETS "${CONNECTOR}"
  RUNTIME DESTINATION "${PREFIX_BINARY}"
)

# Include build package.
include("package.cmake")

# Print summary.
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  Project")
message(STATUS "    - Name                      Centreon Connector ICMP")
message(STATUS "    - Version                   ${CONNECTOR_ICMP_VERSION}")
message(STATUS "")
message(STATUS "  System")
message(STATUS "    - Name                      ${CMAKE_SYSTEM_NAME}")
message(STATUS "    - Version                   ${CMAKE_SYSTEM_VERSION}")
message(STATUS "    - Processor                 ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  Build")
message(STATUS "    - Compiler                  ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "    - Extra compilation flags   ${CMAKE_CXX_FLAGS}")
message(STATUS "    - Build unit tests          ${BUILD_UNIT_TESTS}")
message(STATUS "")
message(STATUS "  Install")
message(STATUS "    - Prefix                    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "    - Binary directory          ${PREFIX_BINARY}")
message(STATUS "    - Package                   ${PACKAGE_LIST}")
message(STATUS "")
