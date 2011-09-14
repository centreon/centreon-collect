##
## Copyright 2011 Merethis
##
## This file is part of Centreon Connector ICMP.
##
## Centreon Connector ICMP is free software: you can redistribute it
## and/or modify it under the terms of the GNU General Public License
## version 2 as published by the Free Software Foundation.
##
## Centreon Connector ICMP is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Centreon Connector ICMP. If not, see
## <http://www.gnu.org/licenses/>.
##

# Global options.
cmake_minimum_required(VERSION 2.8)
project("Centreon Connector ICMP" C CXX)
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INC_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/connector/icmp")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")
include_directories("${PROJECT_SOURCE_DIR}/inc")

# Find Centreon Engine.
if (WITH_ENGINE_INCLUDE_DIR)
  set(ENGINE_INCLUDE_DIR "${WITH_ENGINE_INCLUDE_DIR}")
else ()
  find_path(ENGINE_INCLUDE_DIR NAMES "engine.hh")
  if (NOT ENGINE_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find Centreon Engine development headers (use WITH_ENGINE_INCLUDE_DIR ?).")
  endif ()
endif ()
include_directories("${ENGINE_INCLUDE_DIR}")
if (WITH_ENGINE_CONNECTOR_LIB)
  set(ENGINE_CONNECTOR_LIB "${WITH_ENGINE_CONNECTOR_LIB}")
else ()
  find_library(ENGINE_CONNECTOR_LIB NAMES "cce_connector")
  if (NOT ENGINE_CONNECTOR_LIB)
    message(FATAL_ERROR "Could not find Centreon Engine Connector library (use WITH_ENGINE_CONNECTOR_LIB ?).")
  endif ()
endif ()

# Find Qt.
find_package(Qt4 REQUIRED QtCore)
include("${QT_USE_FILE}")

# Process sources with Qt's moc.
qt4_wrap_cpp(QT_MOC_FILE
  "${INC_DIR}/check.hh"
  "${INC_DIR}/dispatcher.hh")

# ICMP connector library.
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
  # Qt-processed files.
  ${QT_MOC_FILE}
)
target_link_libraries("${CONNECTORLIB}" "${QT_LIBRARIES}")

# ICMP connector.
set(CONNECTOR "centreon_connector_icmp")
add_executable("${CONNECTOR}"
  # Sources.
  "${SRC_DIR}/main.cc"
)
target_link_libraries("${CONNECTOR}"
  "${CONNECTORLIB}"
  ${ENGINE_CONNECTOR_LIB}
  ${QT_LIBRARIES})

# Testing.
option(WITH_TESTING "Build unit tests." OFF)
if (WITH_TESTING)
  # Enable testing.
  enable_testing()
endif ()
