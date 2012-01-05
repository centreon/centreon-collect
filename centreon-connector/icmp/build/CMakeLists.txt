##
## Copyright 2011 Merethis
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

# Version.
set(CONNECTOR_ICMP_MAJOR 0)
set(CONNECTOR_ICMP_MINOR 1)
set(CONNECTOR_ICMP_PATCH 0)
set(CONNECTOR_ICMP_VERSION "${CONNECTOR_ICMP_MAJOR}.${CONNECTOR_ICMP_MINOR}.${CONNECTOR_ICMP_PATCH}")

# Engine version needs.
set(ENGINE_MAJOR 0)
set(ENGINE_MINOR 1)

# Include module to check existing libraries.
include(CheckLibraryExists)

# Include module CTest if necessary.
if (WITH_TESTING)
  include(CTest)
endif ()

# Find centreon clib library.
set(LIB_CENTREON_CLIB "centreon_clib")
# check_library_exists(
#   "${LIB_CENTREON_CLIB}"
#   "clock_gettime"
#   "${CMAKE_LIBRARY_PATH}"
#   FIND_LIB_CENTREON_CLIB
# )
# if (NOT FIND_LIB_CENTREON_CLIB)
#   message(FATAL_ERROR "Could not find centreon clib library.")
# endif ()

# Set directories.
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc")
set(INC_DIR "${INCLUDE_DIR}/com/centreon/connector/icmp")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")

# Set path.
set(PREFIX "${CMAKE_INSTALL_PREFIX}/centreon-connector")
if (WITH_PREFIX)
  set(PREFIX "${WITH_PREFIX}")
  set(CMAKE_INSTALL_PREFIX "${PREFIX}")
endif ()

set(PREFIX_BIN "${PREFIX}/centreon_connector_icmp")
if (WITH_PREFIX_BIN)
  set(bindir "${WITH_PREFIX_BIN}")
endif ()

set(clib_libdir "${CMAKE_INSTALL_PREFIX}/centreon-clib/lib")
if (WITH_CLIB_LIB_DIR)
  set(clib_libdir "${WITH_CLIB_LIB_DIR}")
endif ()

set(clib_incdir "${CMAKE_INSTALL_PREFIX}/centreon-clib/include")
if (WITH_CLIB_INCLUDE_DIR)
  set(clib_incdir "${WITH_CLIB_INCLUDE_DIR}")
endif ()

# Set options.
set(UNIT_TEST "No")
if (WITH_TESTING)
  set(UNIT_TEST "Yes")
endif ()

set(DEB_PACKAGE "No")
if (CPACK_BINARY_DEB)
  set(DEB_PACKAGE "Yes")
endif ()

set(RPM_PACKAGE "No")
if (CPACK_BINARY_RPM)
  set(RPM_PACKAGE "Yes")
endif ()

# Include directories.
include_directories("${INCLUDE_DIR}")
include_directories("${clib_incdir}")

# Link directories.
link_directories("${clib_libdir}")

# Add subdirectories.
if (WITH_TESTING)
  add_subdirectory("test")
endif ()

# Generate version header.
configure_file(
  "${INC_DIR}/version.hh.in"
  "${INC_DIR}/version.hh"
)

# Create project.
add_executable(
  "centreon_connector_icmp"

# Sources.
  "${SRC_DIR}/check.cc"
  "${SRC_DIR}/check_dispatch.cc"
  "${SRC_DIR}/check_observer.cc"
  "${SRC_DIR}/check_options.cc"
  "${SRC_DIR}/cmd_dispatch.cc"
  "${SRC_DIR}/cmd_execute.cc"
  "${SRC_DIR}/cmd_options.cc"
  "${SRC_DIR}/host.cc"
  "${SRC_DIR}/icmp_info.cc"
  "${SRC_DIR}/icmp_socket.cc"
  "${SRC_DIR}/interrupt.cc"
  "${SRC_DIR}/main.cc"
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
  "${INC_DIR}/check_options.hh"
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
  "centreon_connector_icmp"
  "${LIB_CENTREON_CLIB}"
)

# Install project.
install(
  TARGETS "centreon_connector_icmp"
  DESTINATION "${PREFIX_BIN}"
  COMPONENT "runtime"
)

# Include build package.
include("package.cmake")

# Print summary.
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  System                        ${CMAKE_SYSTEM_NAME}")
message(STATUS "    - Version                   ${CMAKE_SYSTEM_VERSION}")
message(STATUS "    - Processor                 ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  Project                       ${PROJECT_NAME}")
message(STATUS "    - Version                   ${CONNECTOR_ICMP_VERSION}")
message(STATUS "    - Extra compilation flags   ${CMAKE_CXX_FLAGS}")
message(STATUS "    - Build unit tests          ${UNIT_TEST}")
message(STATUS "")
message(STATUS "  Installation")
message(STATUS "    - Prefix                    ${PREFIX}")
message(STATUS "    - Binary directory          ${PREFIX_BIN}")
message(STATUS "    - Deb package               ${DEB_PACKAGE}")
message(STATUS "    - RPM package               ${RPM_PACKAGE}")
message(STATUS "")
