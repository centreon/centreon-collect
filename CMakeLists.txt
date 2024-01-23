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
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

find_package(GTest)
find_package(Boost)
find_package(Libssh2)
find_package(mariadb-connector-c)
find_package(opentelemetry-cpp)
find_package(gRPC)
find_package(absl)
find_package(nlohmann_json)
find_package(CURL)
find_package(OpenSSL)
find_package(ryml)
find_package(protobuf)
find_package(spdlog)
find_package(ZLIB)

add_definitions(${spdlog_DEFINITIONS}
${Boost_DEFINITIONS}
${abseil_DEFINITIONS}
${fmt_DEFINITIONS}
${spdlog_DEFINITIONS}
${gtest_DEFINITIONS}
${grpc_DEFINITIONS}
${protobuf_DEFINITIONS}
${openssl_DEFINITIONS}
${zlib_DEFINITIONS}
  )
set(ABSL_LIBRARIES -L${abseil_LIB_DIRS_RELEASE} ${abseil_LIBS_RELEASE})
set(FMT_LIBRARIES -L${fmt_LIB_DIRS_RELEASE} ${fmt_LIBS_RELEASE})
set(SPDLOG_LIBRARIES -L${spdlog_LIB_DIRS_RELEASE} ${spdlog_LIBS_RELEASE})
set(GTEST_LIBRARIES -L${gtest_LIB_DIRS_RELEASE} ${gtest_LIBS_RELEASE})
set(GRPC_LIBRARIES -L${grpc_LIB_DIRS_RELEASE} ${grpc_LIBS_RELEASE})
set(PROTOBUF_LIBRARIES -L${protobuf_LIB_DIRS_RELEASE} ${protobuf_LIBS_RELEASE})
set(OPENSSL_LIBRARIES -L${openssl_LIB_DIRS_RELEASE} ${openssl_LIBS_RELEASE})
set(ZLIB_LIBRARIES -L${zlib_LIB_DIRS_RELEASE} ${zlib_LIBS_RELEASE})

message(STATUS "&&&&&&&&&&&&&&&&&& ${zlib_LIB_DIRS_RELEASE}")
message(STATUS "&&&&&&&&&&&&&&&&&& ${zlib_LIBS_RELEASE}")


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

include_directories("inc")

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

add_definitions(${ryml_DEFINITIONS})

message(STATUS "##### ##################### ${boost_INCLUDE_DIRS_RELEASE}")

include_directories(
  ${absl_INCLUDE_DIRS}
  boost::boost
  ${fmt_INCLUDE_DIRS}
  ${spdlog_INCLUDE_DIRS}
  ${nlohmann_json_INCLUDE_DIRS}
  ${gRPC_INCLUDE_DIRS}
  ${mariadb-connector-c_INCLUDE_DIRS}
  ${opentelemetry-cpp_INCLUDE_DIRS}
  ${c4core_INCLUDE_DIRS}
  ${FastFloat_INCLUDE_DIRS}
  ${ryml_INCLUDE_DIRS}
  )

# Version.
set(COLLECT_MAJOR 24)
set(COLLECT_MINOR 04)
set(COLLECT_PATCH 0)
set(COLLECT_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_VERSION=\"${COLLECT_VERSION}\")

if (DEBUG_ROBOT)
  add_definitions(-DDEBUG_ROBOT)
endif()

# ########### CONSTANTS ###########
set(USER_BROKER centreon-broker)
set(USER_ENGINE centreon-engine)

set(CMAKE_PROGRAM_PATH ${protobuf_BIN_DIRS_RELEASE};${grpc_BIN_DIRS_RELEASE};${CMAKE_PROGRAM_PATH})

message(STATUS "#####################################################")
message(NOTICE "-- use grpc compiler path: ${gRPC_LIBRARIES}")
message(STATUS "#####################################################")

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

include_directories(${CMAKE_SOURCE_DIR} ${CONAN_INCLUDE_DIRS}
                    ${CMAKE_SOURCE_DIR}/clib/inc)

add_subdirectory(bbdo)
add_subdirectory(common)
add_subdirectory(broker)
add_subdirectory(clib)
add_subdirectory(engine)
add_subdirectory(connectors)
add_subdirectory(ccc)

if (WITH_MALLOC_TRACE)
  add_subdirectory(malloc-trace)
endif()

add_custom_target(test-broker COMMAND tests/ut_broker)
add_custom_target(test-engine COMMAND tests/ut_engine)
add_custom_target(test-clib COMMAND tests/ut_clib)
add_custom_target(test-connector COMMAND tests/ut_connector)
add_custom_target(test-common COMMAND tests/ut_common)

add_custom_target(test DEPENDS test-broker test-engine test-clib test-connector
                               test-common)

add_custom_target(test-coverage DEPENDS broker-test-coverage
                                        engine-test-coverage clib-test-coverage)
