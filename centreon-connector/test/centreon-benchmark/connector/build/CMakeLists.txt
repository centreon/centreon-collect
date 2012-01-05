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
project("Centreon Benchmark Connector" C CXX)

# Version.
set(BENCHMARK_CONNECTOR_MAJOR 0)
set(BENCHMARK_CONNECTOR_MINOR 1)
set(BENCHMARK_CONNECTOR_PATCH 0)
set(BENCHMARK_CONNECTOR_VERSION "${BENCHMARK_CONNECTOR_MAJOR}.${BENCHMARK_CONNECTOR_MINOR}.${BENCHMARK_CONNECTOR_PATCH}")

# Include module to check existing libraries.
include(CheckLibraryExists)

# Include module CTest if necessary.
if (WITH_TESTING)
  include(CTest)
endif ()

# Set directories.
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc")
set(INC_DIR "${INCLUDE_DIR}/com/centreon/benchmark/connector")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")

# Set path.
set(PREFIX "${CMAKE_INSTALL_PREFIX}/centreon-benchmark")
if (WITH_PREFIX)
  set(PREFIX "${WITH_PREFIX}")
  set(CMAKE_INSTALL_PREFIX "${PREFIX}")
endif ()

set(PREFIX_BIN "${PREFIX}/centreon_benchmark_connector")
if (WITH_PREFIX_BIN)
  set(bindir "${WITH_PREFIX_BIN}")
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

# Add subdirectories.
if (WITH_TESTING)
  add_subdirectory("test")
endif ()

# Create project.
add_executable(
  "centreon_benchmark_connector"

# Sources.
  "${SRC_DIR}/basic_exception.cc"
  "${SRC_DIR}/benchmark.cc"
  "${SRC_DIR}/connector.cc"
  "${SRC_DIR}/main.cc"
  "${SRC_DIR}/misc.cc"
  "${SRC_DIR}/plugin.cc"

# Headers.
  "${INC_DIR}/basic_exception.hh"
  "${INC_DIR}/benchmark.hh"
  "${INC_DIR}/connector.hh"
  "${INC_DIR}/misc.hh"
  "${INC_DIR}/namespace.hh"
  "${INC_DIR}/plugin.hh"
)
# target_link_libraries(
#   "centreon_benchmark_connector"
# )

# Install project.
install(
  TARGETS "centreon_benchmark_connector"
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
message(STATUS "    - Version                   ${BENCHMARK_CONNECTOR_VERSION}")
message(STATUS "    - Extra compilation flags   ${CMAKE_CXX_FLAGS}")
message(STATUS "    - Build unit tests          ${UNIT_TEST}")
message(STATUS "")
message(STATUS "  Installation")
message(STATUS "    - Prefix                    ${PREFIX}")
message(STATUS "    - Binary directory          ${PREFIX_BIN}")
message(STATUS "    - Deb package               ${DEB_PACKAGE}")
message(STATUS "    - RPM package               ${RPM_PACKAGE}")
message(STATUS "")
