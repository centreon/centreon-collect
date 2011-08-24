##
## Copyright 2011 Merethis
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
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
include_directories("${INC_DIR}")
set(INC_DIR "${INC_DIR}/com/centreon/connector/perl")

# Find Perl.
include(FindPerlLibs)
if (NOT PERLLIBS_FOUND)
  message(FATAL_ERROR "Could not find Perl libraries.")
endif ()
include_directories("${PERL_INCLUDE_PATH}")

# Find ppoll.
include(CheckFunctionExists)
check_function_exists("ppoll" HAVE_PPOLL)
if (HAVE_PPOLL)
  add_definitions(-DHAVE_PPOLL)
endif ()

# Find pthreads.
set(CMAKE_THREAD_PREFER_PTHREAD)
include(FindThreads)
if (NOT CMAKE_USE_PTHREADS_INIT)
  message(FATAL_ERROR "Could not find pthreads.")
endif ()
set(THREAD_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")

# Perl connector.
set(CONNECTOR "centreon_connector_perl")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/check_terminated.cc"
  "${SRC_DIR}/embedded.cc"
  "${SRC_DIR}/main.cc"
  "${SRC_DIR}/main_io.cc"
  "${SRC_DIR}/multiplex.cc"
  "${SRC_DIR}/process.cc"
  "${SRC_DIR}/processes.cc"
  "${SRC_DIR}/script.cc"
  # Headers.
  "${INC_DIR}/array_ptr.hh"
  "${INC_DIR}/check_terminated.hh"
  "${INC_DIR}/embedded.hh"
  "${INC_DIR}/main_io.hh"
  "${INC_DIR}/multiplex.hh"
  "${INC_DIR}/process.hh"
  "${INC_DIR}/processes.hh"
)
target_link_libraries("${CONNECTOR}"
  "${PERL_LIBRARY}"
  "${THREAD_LIBRARIES}")
