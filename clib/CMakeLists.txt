#
# Copyright 2011-2014,2018-2021 Centreon
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

# Global options.
cmake_minimum_required(VERSION 3.18)
project("Centreon Clib" C CXX)

# Set directories.
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc")
set(INC_DIR "${INCLUDE_DIR}/com/centreon")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(SCRIPT_DIR "${PROJECT_SOURCE_DIR}/script")

# Version.
set(CLIB_VERSION "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")

# Include module to check existing libraries.
include(CheckLibraryExists)
# Include module CTest if necessary.
if(WITH_TESTING)
  include(CTest)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD)
include(FindThreads)
if(NOT CMAKE_USE_PTHREADS_INIT)
  message(FATAL_ERROR "Could not find pthreads library.")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" AND CMAKE_COMPILER_IS_GNUCXX)
  set(LIB_THREAD "-pthread -lpthread")
else()
  set(LIB_THREAD "${CMAKE_THREAD_LIBS_INIT}")
endif()

# Find real time library.
if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(LIB_RT "")
else()
  set(LIB_RT "rt")
endif()

check_library_exists("${LIB_RT}" "clock_gettime" "${CMAKE_LIBRARY_PATH}"
                     FIND_LIB_RT)
if(NOT FIND_LIB_RT)
  message(FATAL_ERROR "Could not find real time library.")
endif()

# Find dynamic linking library.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LIB_DL "dl")
elseif(
  CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"
  OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD"
  OR CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  set(LIB_DL "c")
else()
  set(LIB_DL "")
endif()

check_library_exists("${LIB_DL}" "dlopen" "${CMAKE_LIBRARY_PATH}" FIND_LIB_DL)
if(NOT FIND_LIB_DL)
  message(FATAL_ERROR "Could not find dynamic linking library.")
endif()

# Set path.
if(WITH_PREFIX_LIB_CLIB)
  set(PREFIX_LIB "${WITH_PREFIX_LIB_CLIB}")
else()
  set(PREFIX_LIB "${CMAKE_INSTALL_PREFIX}/lib")
endif()

set(UNIT_TEST "No")
if(WITH_TESTING)
  set(UNIT_TEST "Yes")
endif()

set(DEB_PACKAGE "No")
if(CPACK_BINARY_DEB)
  set(DEB_PACKAGE "Yes")
endif()

set(RPM_PACKAGE "No")
if(CPACK_BINARY_RPM)
  set(RPM_PACKAGE "Yes")
endif()

# Find headers.
include(CheckIncludeFileCXX)
check_include_file_cxx("spawn.h" HAVE_SPAWN_H)
if(HAVE_SPAWN_H)
  add_definitions(-DHAVE_SPAWN_H)
else()
  message(WARNING "Your plateform does not have spawn.h. Some features might "
                  "be disabled.")
endif()

# Set sources.
set(SOURCES
    "${SRC_DIR}/library.cc"
    "${SRC_DIR}/process_manager.cc"
    "${SRC_DIR}/process.cc"
    "${SRC_DIR}/handle_manager.cc"
    "${SRC_DIR}/handle_action.cc"
    "${SRC_DIR}/task_manager.cc"
    "${SRC_DIR}/timestamp.cc")

# Set headers.
set(HEADERS
    ${SPECIFIC_HEADERS}
    "${INC_DIR}/clib.hh"
    "${INC_DIR}/handle.hh"
    "${INC_DIR}/handle_action.hh"
    "${INC_DIR}/handle_listener.hh"
    "${INC_DIR}/handle_manager.hh"
    "${INC_DIR}/hash.hh"
    "${INC_DIR}/library.hh"
    "${INC_DIR}/process_manager.hh"
    "${INC_DIR}/process.hh"
    "${INC_DIR}/task.hh"
    "${INC_DIR}/task_manager.hh"
    "${INC_DIR}/timestamp.hh"
    "${INC_DIR}/unique_array_ptr.hh"
    "${INC_DIR}/unordered_hash.hh")

# Include directories.
include_directories("${INCLUDE_DIR}")

# Add subdirectories.
add_subdirectory(src/clib)
add_subdirectory(src/logging)
add_subdirectory(src/misc)
add_subdirectory(src/io)
if(WITH_TESTING AND NOT ONLY_ROBOT_CMA)
  add_subdirectory(test)
endif()

# Create shared library.
add_library(centreon_clib SHARED ${SOURCES} ${HEADERS})
# Link target with required libraries.
target_link_libraries(centreon_clib ${LIB_THREAD} ${LIB_RT} ${LIB_DL} fmt::fmt)
# Set output name for the shared library.
set_target_properties(centreon_clib PROPERTIES OUTPUT_NAME centreon_clib)
# Install shared library.
install(
  TARGETS centreon_clib
  DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
  COMPONENT "runtime")

# Install header files for devel.
install(
  DIRECTORY "${INCLUDE_DIR}/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/centreon-clib"
  COMPONENT "development"
  FILES_MATCHING
  PATTERN "*.hh")

# Include build package.
include(cmake/package.cmake)

# Print summary.
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  Project")
message(STATUS "    - Name                      ${PROJECT_NAME}")
message(STATUS "    - Version                   ${CLIB_VERSION}")
message(STATUS "")
message(STATUS "  System")
message(STATUS "    - Name                      ${CMAKE_SYSTEM_NAME}")
message(STATUS "    - Version                   ${CMAKE_SYSTEM_VERSION}")
message(STATUS "    - Processor                 ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  Build")
message(STATUS "    - Compiler                  ${CMAKE_CXX_COMPILER} "
               "(${CMAKE_CXX_COMPILER_ID})")
message(STATUS "    - Extra compilation flags   ${CMAKE_CXX_FLAGS}")
message(STATUS "    - Build unit tests          ${UNIT_TEST}")
message(STATUS "")
message(STATUS "  Installation")
message(STATUS "    - Prefix                    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "    - Library directory         ${PREFIX_LIB}")
message(STATUS "    - Package                   ${PACKAGE_LIST}")
message(STATUS "")
