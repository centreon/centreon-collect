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

set(CMAKE_CXX_STANDARD 17)

string(TIMESTAMP CENTREON_CURRENT_YEAR "%Y")
add_definitions(-DCENTREON_CURRENT_YEAR="${CENTREON_CURRENT_YEAR}")

if(DEFINED ENV{VCPKG_ROOT})
  set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
  message(
    STATUS "TOOLCHAIN set to ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE
      "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
else()
  message(
    STATUS
      "TOOLCHAIN set to ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  )
  set(CMAKE_TOOLCHAIN_FILE
      "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project("Centreon Collect" C CXX)

option(WITH_ASAN
       "Add the libasan to check memory leaks and other memory issues." OFF)

option(WITH_TSAN
       "Add the libtsan to check threads and other multithreading issues." OFF)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID
                                                STREQUAL "Clang")
  message(
    FATAL_ERROR "You can build broker with g++ or clang++. CMake will exit.")
endif()

option(WITH_MALLOC_TRACE "compile centreon-malloc-trace library." OFF)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++")
# set(CMAKE_CXX_COMPILER "clang++")
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")

option(DEBUG_ROBOT OFF)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WITH_TSAN)
  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=thread")
  set(CMAKE_LINKER_FLAGS_DEBUG
      "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=thread")
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
set(COLLECT_MAJOR 24)
set(COLLECT_MINOR 07)
set(COLLECT_PATCH 0)
set(COLLECT_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_VERSION=\"${COLLECT_VERSION}\")

if(DEBUG_ROBOT)
  add_definitions(-DDEBUG_ROBOT)
endif()

# ########### CONSTANTS ###########
set(USER_BROKER centreon-broker)
set(USER_ENGINE centreon-engine)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS url)
find_package(ryml CONFIG REQUIRED)
add_definitions("-DSPDLOG_FMT_EXTERNAL")

include(FindPkgConfig)
pkg_check_modules(MARIADB REQUIRED libmariadb)
pkg_check_modules(LIBSSH2 REQUIRED libssh2)

# There is a bug with grpc. It is not put in the triplet directory. So we have
# to search for its plugin.
file(
  GLOB_RECURSE GRPC_CPP_PLUGIN_EXE
  RELATIVE ${CMAKE_BINARY_DIR}
  grpc_cpp_plugin)
find_program(
  GRPC_CPP_PLUGIN
  NAMES ${GRPC_CPP_PLUGIN_EXE}
  PATHS ${CMAKE_BINARY_DIR} REQUIRED
  NO_DEFAULT_PATH)

set(PROTOBUF_LIB_DIR ${Protobuf_DIR}/../../lib)
set(OTLP_LIB_DIR ${opentelemetry-cpp_DIR}/../../lib)
set(VCPKG_INCLUDE_DIR ${Protobuf_INCLUDE_DIR})
include(GNUInstallDirs)

# import opentelemetry-proto
add_custom_command(
  OUTPUT
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/collector/metrics/v1/metrics_service.proto
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/metrics/v1/metrics.proto
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/common/v1/common.proto
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/resource/v1/resource.proto
  COMMENT "get opentelemetry proto files from git repository"
  COMMAND /bin/rm -rf ${CMAKE_SOURCE_DIR}/opentelemetry-proto
  COMMAND
    git ARGS clone --depth=1 --single-branch
    https://github.com/open-telemetry/opentelemetry-proto.git
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target(
  opentelemetry-proto-files
  DEPENDS
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/collector/metrics/v1/metrics_service.proto
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/metrics/v1/metrics.proto
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/common/v1/common.proto
    ${CMAKE_SOURCE_DIR}/opentelemetry-proto/opentelemetry/proto/resource/v1/resource.proto
)

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
add_definitions(-DDEFAULT_COMMAND_FILE="${ENGINE_VAR_LIB_DIR}/rw/centengine.cmd"
                -DDEFAULT_DEBUG_FILE="${ENGINE_VAR_LOG_DIR}/centengine.debug"
		-DDEFAULT_LOG_FILE="${ENGINE_VAR_LOG_DIR}/centengine.log"
                -DDEFAULT_RETENTION_FILE="${ENGINE_VAR_LOG_DIR}/retention.dat"
                -DDEFAULT_STATUS_FILE="${ENGINE_VAR_LOG_DIR}/status.dat")

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

include_directories(${CMAKE_SOURCE_DIR} ${VCPKG_INCLUDE_DIR} fmt::fmt
                    spdlog::spdlog ${CMAKE_SOURCE_DIR}/clib/inc)

add_subdirectory(clib)
add_subdirectory(common)
add_subdirectory(broker)
add_subdirectory(bbdo)
add_subdirectory(engine)
add_subdirectory(connectors)
add_subdirectory(ccc)
add_subdirectory(agent)

if(WITH_MALLOC_TRACE)
  add_subdirectory(malloc-trace)
endif()

add_custom_target(test-broker COMMAND tests/ut_broker)
add_custom_target(test-engine COMMAND tests/ut_engine)
add_custom_target(test-clib COMMAND tests/ut_clib)
add_custom_target(test-connector COMMAND tests/ut_connector)
add_custom_target(test-common COMMAND tests/ut_common)
add_custom_target(test-agent COMMAND tests/ut_agent)

add_custom_target(test DEPENDS test-broker test-engine test-clib test-connector
                               test-common test-agent)

add_custom_target(test-coverage DEPENDS broker-test-coverage
                                        engine-test-coverage clib-test-coverage)
