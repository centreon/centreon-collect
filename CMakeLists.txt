##
## Copyright 2009-2022 Centreon
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
#
# Copyright 2009-2022 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

#
# Global settings.
#

# Set necessary settings.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.16)
project("Centreon Collect" C CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID
  STREQUAL "Clang")
  message(
    FATAL_ERROR "You can build broker with g++ or clang++. CMake will exit.")
endif()

set(ALLOW_DUPLICATE_EXECUTABLE TRUE)
set(CMAKE_CXX_STANDARD 14)

# set -latomic if OS is Raspbian.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

# Version.
set(COLLECT_MAJOR 21)
set(COLLECT_MINOR 10)
set(COLLECT_PATCH 4)
set(COLLECT_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_VERSION=\"${COLLECT_VERSION}\")

# add_definitions(-DCENTREON_BROKER_VERSION=\"${COLLECT_VERSION}\")

# ########### CONSTANTS ###########
set(USER_BROKER centreon-broker)
set(USER_ENGINE centreon-engine)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# ##############################################################################
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(CMAKE_PROGRAM_PATH ${CONAN_BIN_DIRS_PROTOBUF};${CMAKE_PROGRAM_PATH})

find_package(Protobuf REQUIRED)

message(NOTICE "-- use protoc compiler: ${Protobuf_PROTOC_EXECUTABLE}")

include_directories(${CONAN_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/centreon-clib/inc)
set(CMAKE_INSTALL_PREFIX "/usr")
add_subdirectory(centreon-broker)
add_subdirectory(centreon-clib)
add_subdirectory(centreon-engine)
add_subdirectory(centreon-connector)

add_custom_target(test-broker COMMAND test/ut_broker)
add_custom_target(test-engine COMMAND tests/ut_engine)
add_custom_target(test-clib COMMAND tests/ut_clib)

# add_custom_target(test-connector COMMAND ut_connector)
add_custom_target(test DEPENDS test-broker test-engine test-clib test-connector)
