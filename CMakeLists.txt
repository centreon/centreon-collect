#
# Copyright 2009-2023 Centreon
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
set(CMAKE_COLOR_DIAGNOSTICS ON)
cmake_minimum_required(VERSION 3.16)
project("Centreon Collect" C CXX)

option(WITH_CLANG "Compilation with clang" OFF)

# With libasan
option(WITH_ASAN
       "Add the libasan to check memory leaks and other memory issues." OFF)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID
                                                STREQUAL "Clang")
  message(
    FATAL_ERROR "You can build broker with g++ or clang++. CMake will exit.")
endif()

option(WITH_CLANG "Compilation with clang" OFF)

add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
option(NG "C++17 build." OFF)

if(NG)
  set(CMAKE_CXX_STANDARD 17)
else()
  set(CMAKE_CXX_STANDARD 14)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WITH_CLANG)
  set(CMAKE_C_COMPILER "/usr/bin/clang")

  find_program(CLANG_CPP "/usr/bin/clang++")
  if(NOT CLANG_CPP_FOUND)
    set(CMAKE_CXX_COMPILER "/usr/bin/clang")
  else()
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
  endif()
endif()

if(WITH_ASAN)
  set(CMAKE_BUILD_TYPE Debug)
  if(WITH_CLANG)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG
        "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
  else()
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG
        "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address"
    )
  endif()
endif()

set(ALLOW_DUPLICATE_EXECUTABLE TRUE)

set(BUILD_ARGS "-w" "dupbuild=warn")

#
# Get distributions name
#
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(STRINGS "/etc/os-release" release REGEX "^ID")

  foreach(line ${release})
    if(${line} MATCHES "ID_LIKE=.*")
      string(REGEX REPLACE "ID_LIKE=\"(.*)\"" "\\1" like ${line})
    endif()

    if(${line} MATCHES "ID=.*")
      string(REGEX REPLACE "ID=\"(.*)\"" "\\1" id ${line})
    endif()
  endforeach()

  string(TOLOWER "${like}" like)
  string(TOLOWER "${id}" id)

  if(("${id}" MATCHES "debian")
     OR ("${like}" MATCHES "debian")
     OR ("${id}" MATCHES "ubuntu")
     OR ("${like}" MATCHES "ubuntu"))
    set(OS_DISTRIBUTOR "Debian")
  elseif(("${id}" MATCHES "centos") OR ("${like}" MATCHES "centos"))
    set(OS_DISTRIBUTOR "CentOS")
  else()
    message(WARNING "lsb_release in not installed")
    set(OS_DISTRIBUTOR "${CMAKE_SYSTEM_NAME}")
  endif()
else()
  set(OS_DISTRIBUTOR "${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "${id} detected (compatible with ${OS_DISTRIBUTOR})")

# set -latomic if OS is Raspbian.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

# Version.
set(COLLECT_MAJOR 23)
set(COLLECT_MINOR 04)
set(COLLECT_PATCH 0)
set(COLLECT_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_VERSION=\"${COLLECT_VERSION}\")

# ########### CONSTANTS ###########
set(USER_BROKER centreon-broker)
set(USER_ENGINE centreon-engine)

# ##############################################################################
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(CMAKE_PROGRAM_PATH ${CONAN_BIN_DIRS_PROTOBUF};${CMAKE_PROGRAM_PATH})

find_package(Protobuf REQUIRED)

message(NOTICE "-- use protoc compiler: ${Protobuf_PROTOC_EXECUTABLE}")

include(GNUInstallDirs)

# var directories.
set(BROKER_VAR_LOG_DIR
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/centreon-broker")
set(BROKER_VAR_LIB_DIR
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/centreon-broker")
set(ENGINE_VAR_LOG_DIR
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/centreon-engine")
set(ENGINE_VAR_LOG_ARCHIVE_DIR
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/centreon-engine/archives")
set(ENGINE_VAR_LIB_DIR
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/centreon-engine")

set(CMAKE_INSTALL_PREFIX "/usr")
option(WITH_TESTING "Build unit tests." OFF)

option(WITH_CONF "Install configuration files." ON)

# Code coverage on unit tests
option(WITH_COVERAGE "Add code coverage on unit tests." OFF)

if(WITH_TESTING AND WITH_COVERAGE)
  set(CMAKE_BUILD_TYPE "Debug")
  include(cmake/CodeCoverage.cmake)
  append_coverage_compiler_flags()
endif()

set(protobuf_MODULE_COMPATIBLE True)

add_definitions(${CONAN_DEFINES_SPDLOG})

include_directories(${CMAKE_SOURCE_DIR} ${CONAN_INCLUDE_DIRS}
                    ${CMAKE_SOURCE_DIR}/clib/inc)

add_subdirectory(bbdo)
add_subdirectory(broker)
add_subdirectory(clib)
add_subdirectory(engine)
add_subdirectory(connectors)
add_subdirectory(ccc)

add_custom_target(test-broker COMMAND tests/ut_broker)
add_custom_target(test-engine COMMAND tests/ut_engine)
add_custom_target(test-clib COMMAND tests/ut_clib)
add_custom_target(test-connector COMMAND tests/ut_connector)

add_custom_target(test DEPENDS test-broker test-engine test-clib test-connector)

add_custom_target(test-coverage DEPENDS broker-test-coverage
                                        engine-test-coverage clib-test-coverage)
