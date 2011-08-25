##
## Copyright 2011 Merethis
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
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
include_directories("${INC_DIR}")
set(INC_DIR "${INC_DIR}/com/centreon/connector/ssh")

# Find libssh2's headers.
find_path(LIBSSH2_INCLUDE_DIR "libssh2.h")
if (NOT LIBSSH2_INCLUDE_DIR)
  message(FATAL_ERROR "Could not find libssh2's headers.")
else ()
  include_directories("${LIBSSH2_INCLUDE_DIR}")
endif ()

# Find libssh2's library.
find_library(LIBSSH2_LIBRARIES "ssh2")
if (NOT LIBSSH2_LIBRARIES)
  message(FATAL_ERROR "Could not find libssh2's library.")
endif ()

# SSH connector.
set(CONNECTOR "centreon_connector_ssh")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/channel.cc"
  "${SRC_DIR}/credentials.cc"
  "${SRC_DIR}/exception.cc"
  "${SRC_DIR}/main.cc"
  "${SRC_DIR}/multiplex.cc"
  "${SRC_DIR}/session.cc"
  "${SRC_DIR}/sessions.cc"
  "${SRC_DIR}/std_io.cc"
  # Headers.
  "${INC_DIR}/array_ptr.hh"
  "${INC_DIR}/channel.hh"
  "${INC_DIR}/credentials.hh"
  "${INC_DIR}/exception.hh"
  "${INC_DIR}/multiplex.hh"
  "${INC_DIR}/session.hh"
  "${INC_DIR}/sessions.hh"
  "${INC_DIR}/std_io.hh"
)
target_link_libraries("${CONNECTOR}" "${LIBSSH2_LIBRARIES}")
