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
project("Centreon Connector icmp" CXX)
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
# set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")

if (WITH_ENGINE_INCLUDE_DIR)
  set(engine_incdir "${WITH_ENGINE_INCLUDE_DIR}")
else ()
  find_path(engine_incdir "centreon-engine")
  set(engine_incdir "${engine_incdir}/centreon-engine")
endif ()

# include.
include_directories("${engine_incdir}")
include_directories("${INC_DIR}")
set(INC_DIR "${INC_DIR}/com/centreon/connector/icmp")

# Find Qt.
find_package(Qt4 COMPONENTS QtCore REQUIRED)
include(${QT_USE_FILE})

# icmp connector library.
set(CONNECTORLIB "centreonconnectoricmp")
add_library("${CONNECTORLIB}"
  # Sources.
  "${SRC_DIR}/check.cc"
  "${SRC_DIR}/dispatcher.cc"
  "${SRC_DIR}/socket_manager.cc"

  # Headers.
  "${INC_DIR}/check.hh"
  "${INC_DIR}/dispatcher.hh"
  "${INC_DIR}/socket_manager.hh"
)
target_link_libraries("${CONNECTORLIB}" ${QT_LIBRARIES})

# SSH connector.
set(CONNECTOR "centreon_connector_icmp")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/main.cc"
)
target_link_libraries("${CONNECTOR}" "${CONNECTORLIB}")

# # Testing.
# option(WITH_TESTING "Build unit tests." OFF)
# if (WITH_TESTING)
#   # Enable testing.
#   enable_testing()

#   # Exception tests.
#   #   Default constructor.
#   set(TEST_NAME "")
#   add_executable("${TEST_NAME}"
#     "${TEST_DIR}/.cc")
#   target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
#   add_test("${TEST_NAME}" "${TEST_NAME}")
# endif ()
