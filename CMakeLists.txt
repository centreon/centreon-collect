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

#when we build from cache(CI), we don't use vcpkg because it recompiles often everything
if (NOT BUILD_FROM_CACHE)
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

endif()

project("Centreon Collect" C CXX)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++")
# set(CMAKE_CXX_COMPILER "clang++")
add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")

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

message(STATUS "${id} detected (compatible with ${OS_DISTRIBUTOR})")

# set -latomic if OS is Raspbian.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

# Version.
set(COLLECT_MAJOR 24)
set(COLLECT_MINOR 04)
set(COLLECT_PATCH 12)
set(COLLECT_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")


if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  include(CMakeListsLinux.txt)
else()
  include(CMakeListsWindows.txt)
endif()
