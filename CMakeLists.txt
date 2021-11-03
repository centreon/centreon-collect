##
## Copyright 2009-2021 Centreon
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
# Global settings.
#

# Set necessary settings.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 2.8)
project("Centreon Collect" C CXX)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "You can build broker with g++ or clang++. CMake will exit.")
endif ()
set(ALLOW_DUPLICATE_EXECUTABLE TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

set(BUILD_ARGS "-w" "dupbuild=warn")

# set -latomic if OS is Raspbian.
if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif ()

# Version.
set(COLLECT_MAJOR 22)
set(COLLECT_MINOR 04)
set(COLLECT_PATCH 0)
set(COLLECT_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")
add_definitions(-DCENTREON_CONNECTOR_VERSION=\"${COLLECT_VERSION}\")
# add_definitions(-DCENTREON_BROKER_VERSION=\"${COLLECT_VERSION}\")


############ CONSTANTS ###########
set(USER_BROKER centreon-broker)
set(USER_ENGINE centreon-engine)
##################################

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

option(WITH_TESTING "Build unit tests." OFF)
if (WITH_TESTING)
  find_package(GTest REQUIRED)
endif ()

set(protobuf_MODULE_COMPATIBLE True)

find_package(asio REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(c-ares REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

add_definitions(${spdlog_DEFINITIONS})

include_directories(${nlohmann_json_INCLUDE_DIRS})
include_directories(${fmt_INCLUDE_DIRS})
include_directories(${spdlog_INCLUDE_DIRS})
include_directories(${asio_INCLUDE_DIRS})
include_directories(${protobuf_INCLUDE_DIRS})
include_directories(${absl_INCLUDE_DIRS})
include_directories(${gRPC_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR})

link_directories(${nlohmann_json_LIB_DIRS})
link_directories(${fmt_LIB_DIRS})
link_directories(${spdlog_LIB_DIRS})
link_directories(${protobuf_LIB_DIRS})
link_directories(${gRPC_LIB_DIRS})
link_directories(${c-ares_LIB_DIRS})
link_directories(${OpenSSL_LIB_DIRS})
link_directories(${ZLIB_LIB_DIRS})

set(PROTOBUF_PREFIX "${protobuf_LIB_DIRS}/..")

add_subdirectory(protobuf)
add_subdirectory(centreon-broker)
add_subdirectory(centreon-clib)
add_subdirectory(centreon-engine)
add_subdirectory(centreon-connector)

add_custom_target(test-broker
    WORKING_DIRECTORY centreon-broker
    COMMAND test/ut
    )
add_custom_target(test-engine
    WORKING_DIRECTORY centreon-engine
    COMMAND tests/ut_engine
    )
add_custom_target(test-clib
    WORKING_DIRECTORY centreon-clib
    COMMAND test/ut-clib
    )
add_custom_target(test-connector
    WORKING_DIRECTORY centreon-connector
    COMMAND ut_connector
    )

add_custom_target(test
    DEPENDS test-broker test-engine test-clib test-connector
    )
