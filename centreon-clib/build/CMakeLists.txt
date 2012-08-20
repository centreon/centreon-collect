##
## Copyright 2011-2012 Merethis
##
## This file is part of Centreon Clib.
##
## Centreon Clib is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Clib is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Clib. If not, see
## <http://www.gnu.org/licenses/>.
##

# Global options.
cmake_minimum_required(VERSION 2.8)
project("Centreon Clib" C CXX)


# Set directories.
set(PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/..")
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc")
set(INC_DIR "${INCLUDE_DIR}/com/centreon")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(SCRIPT_DIR "${PROJECT_SOURCE_DIR}/script")


# Version.
set(CLIB_MAJOR 1)
set(CLIB_MINOR 0)
set(CLIB_PATCH 0)
set(CLIB_VERSION "${CLIB_MAJOR}.${CLIB_MINOR}.${CLIB_PATCH}")


# Include module to check existing libraries.
include(CheckLibraryExists)

# Include module CTest if necessary.
if (WITH_TESTING)
  include(CTest)
endif ()


# Disable some unnecessary warnings on Windows by using secure
# overloads.
if (WIN32)
  add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)
endif ()


# Find pthreads.
if (WIN32)
  include(FindThreads)
  if (NOT CMAKE_USE_WIN32_THREADS_INIT)
    message(FATAL_ERROR "Could not find Win32 threads.")
  endif ()
else ()
  set(CMAKE_THREAD_PREFER_PTHREAD)
  include(FindThreads)
  if (NOT CMAKE_USE_PTHREADS_INIT)
    message(FATAL_ERROR "Could not find pthreads library.")
  endif ()
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" AND CMAKE_COMPILER_IS_GNUCXX)
  set(LIB_THREAD "-pthread -lpthread")
else ()
  set(LIB_THREAD "${CMAKE_THREAD_LIBS_INIT}")
endif ()

# Find real time library.
if (CMAKE_SYSTEM_NAME STREQUAL "OpenBSD"
    OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(LIB_RT "")
else ()
  set(LIB_RT "rt")
endif ()

if (NOT WIN32)
  check_library_exists(
    "${LIB_RT}"
    "clock_gettime"
    "${CMAKE_LIBRARY_PATH}"
    FIND_LIB_RT
    )
  if (NOT FIND_LIB_RT)
    message(FATAL_ERROR "Could not find real time library.")
  endif ()
endif ()

# Find dynamic linking library.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LIB_DL "dl")
elseif (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"
        OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD"
        OR CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  set(LIB_DL "c")
else ()
  set(LIB_DL "")
endif ()

if (NOT WIN32)
  check_library_exists(
    "${LIB_DL}"
    "dlopen"
    "${CMAKE_LIBRARY_PATH}"
    FIND_LIB_DL
    )
  if (NOT FIND_LIB_DL)
    message(FATAL_ERROR "Could not find dynamic linking library.")
  endif ()
endif ()


# Set path.
if (WITH_PREFIX)
  set(PREFIX "${WITH_PREFIX}")
  set(CMAKE_INSTALL_PREFIX "${PREFIX}")
else ()
  set(PREFIX "${CMAKE_INSTALL_PREFIX}")
endif ()
if (WITH_PREFIX_LIB)
  set(PREFIX_LIB "${WITH_PREFIX_LIB}")
else ()
  set(PREFIX_LIB "${CMAKE_INSTALL_PREFIX}/lib")
endif ()
if (WITH_PREFIX_INC)
  set(PREFIX_INC "${WITH_PREFIX_INC}")
else ()
  set(PREFIX_INC "${CMAKE_INSTALL_PREFIX}/include")
endif ()


# Set pkg-config options.
option(WITH_PKGCONFIG_SCRIPT "Generate and install pkg-config script." ON)
if (WITH_PKGCONFIG_SCRIPT)
  # Generate pkg-config file.
  message(STATUS "Generating pkg-config file.")
  configure_file(
    "${SCRIPT_DIR}/centreon-clib.pc.in"
    "${SCRIPT_DIR}/centreon-clib.pc"
    @ONLY
  )

  # pkg-config file install directory.
  if (WITH_PKGCONFIG_DIR)
    set(PKGCONFIG_DIR "${WITH_PKGCONFIG_DIR}")
  else ()
    set(PKGCONFIG_DIR "/usr/lib/pkgconfig")
  endif ()

  # Install rule.
  install(
    FILES "${SCRIPT_DIR}/centreon-clib.pc"
    DESTINATION "${PKGCONFIG_DIR}"
    COMPONENT "runtime"
  )
endif ()


# Set options.
set(UNIT_TEST "No")
if (WITH_TESTING)
  set(UNIT_TEST "Yes")
endif ()

set(DEB_PACKAGE "No")
if (CPACK_BINARY_DEB)
  set(DEB_PACKAGE "Yes")
endif ()

set(RPM_PACKAGE "No")
if (CPACK_BINARY_RPM)
  set(RPM_PACKAGE "Yes")
endif ()


# Set libraries.
if (NOT WITH_SHARED_LIB AND NOT WITH_STATIC_LIB)
  set(WITH_SHARED_LIB 1)
endif ()

set(SHARED_LIB "No")
if (WITH_SHARED_LIB)
  set(SHARED_LIB "Yes")
endif()

set(STATIC_LIB "No")
if (WITH_STATIC_LIB)
  set(STATIC_LIB "Yes")
endif ()

if (WITH_SHARED_LIB)
  set(DEFAULT_LINK_NAME "centreon_clib_shared")
else ()
  set(DEFAULT_LINK_NAME "centreon_clib_static")
endif ()


# Find headers.
include(CheckIncludeFileCXX)
check_include_file_cxx("spawn.h" HAVE_SPAWN_H)
if (HAVE_SPAWN_H)
  add_definitions(-DHAVE_SPAWN_H)
else ()
  message(WARNING "Your plateform does not have spawn.h. Some features might be disabled.")
endif ()

# Platform-specific files.
if (WIN32)
  set(SPECIFIC_SOURCES
    "${SRC_DIR}/handle_manager_win32.cc"
    "${SRC_DIR}/library_win32.cc"
    "${SRC_DIR}/process_manager_win32.cc"
    "${SRC_DIR}/process_win32.cc"
  )
  set(SPECIFIC_HEADERS
    "${INC_DIR}/handle_manager_win32.hh"
    "${INC_DIR}/library_win32.hh"
    "${INC_DIR}/process_manager_win32.hh"
    "${INC_DIR}/process_win32.hh"
  )
else ()
  set(SPECIFIC_SOURCES
    "${SRC_DIR}/handle_manager_posix.cc"
    "${SRC_DIR}/library_posix.cc"
    "${SRC_DIR}/process_manager_posix.cc"
    "${SRC_DIR}/process_posix.cc"
  )
  set(SPECIFIC_HEADERS
    "${INC_DIR}/handle_manager_posix.hh"
    "${INC_DIR}/library_posix.hh"
    "${INC_DIR}/process_manager_posix.hh"
    "${INC_DIR}/process_posix.hh"
  )
endif ()

# Set sources.
set(
  SOURCES
  ${SPECIFIC_SOURCES}
  "${SRC_DIR}/clib.cc"
  "${SRC_DIR}/handle.cc"
  "${SRC_DIR}/handle_action.cc"
  "${SRC_DIR}/handle_listener.cc"
  "${SRC_DIR}/handle_manager.cc"
  "${SRC_DIR}/task.cc"
  "${SRC_DIR}/task_manager.cc"
  "${SRC_DIR}/timestamp.cc"
)


# Set headers.
set(
  HEADERS
  ${SPECIFIC_HEADERS}
  "${INC_DIR}/clib.hh"
  "${INC_DIR}/delayed_delete.hh"
  "${INC_DIR}/handle.hh"
  "${INC_DIR}/handle_action.hh"
  "${INC_DIR}/handle_listener.hh"
  "${INC_DIR}/handle_manager.hh"
  "${INC_DIR}/library.hh"
  "${INC_DIR}/namespace.hh"
  "${INC_DIR}/process_manager.hh"
  "${INC_DIR}/process.hh"
  "${INC_DIR}/shared_ptr.hh"
  "${INC_DIR}/task.hh"
  "${INC_DIR}/task_manager.hh"
  "${INC_DIR}/timestamp.hh"
  "${INC_DIR}/unique_array_ptr.hh"
  "${INC_DIR}/unordered_hash.hh"
)


# Include directories.
include_directories("${INCLUDE_DIR}")


# Add subdirectories.
add_subdirectory("clib")
add_subdirectory("concurrency")
add_subdirectory("exceptions")
add_subdirectory("logging")
add_subdirectory("misc")
add_subdirectory("io")
if (WITH_TESTING)
  add_subdirectory("test")
endif ()


if (WITH_SHARED_LIB)
  # Create shared library.
  add_library(
    "centreon_clib_shared"
    SHARED
    ${SOURCES}
    ${HEADERS}
  )
  # Link target with required libraries.
  target_link_libraries(
    "centreon_clib_shared"
    ${LIB_THREAD}
    ${LIB_RT}
    ${LIB_DL}
  )
  # Set output name for the shared library.
  set_target_properties(
    "centreon_clib_shared"
    PROPERTIES
    OUTPUT_NAME
    "centreon_clib"
  )
  # Install shared library.
  install(
    TARGETS "centreon_clib_shared"
    DESTINATION "${PREFIX_LIB}"
    COMPONENT "runtime"
  )
endif ()


if (WITH_STATIC_LIB)
  # Create static library.
  add_library(
    "centreon_clib_static"
    STATIC
    ${SOURCES}
    ${HEADERS}
  )
  # Link target with required libraries.
  target_link_libraries(
    "centreon_clib_static"
    ${LIB_THREAD}
    ${LIB_RT}
    ${LIB_DL}
  )
  # Set output name for the static library.
  set_target_properties(
    "centreon_clib_static"
    PROPERTIES
    OUTPUT_NAME
    "centreon_clib"
  )
  # Install static library.
  install(
    TARGETS "centreon_clib_static"
    DESTINATION "${PREFIX_LIB}"
    COMPONENT "runtime"
  )
endif ()


# Install header files for devel.
install(
  DIRECTORY "${INCLUDE_DIR}/"
  DESTINATION "${PREFIX_INC}"
  COMPONENT "development"
  FILES_MATCHING PATTERN "*.hh"
)


# Include build package.
include("package.cmake")

# Print summary.
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  Project")
message(STATUS "    - Name                      ${PROJECT_NAME}")
message(STATUS "    - Version                   ${CLIB_VERSION}")
message(STATUS "    - With shared library       ${SHARED_LIB}")
message(STATUS "    - With static library       ${STATIC_LIB}")
message(STATUS "")
message(STATUS "  System")
message(STATUS "    - Name                      ${CMAKE_SYSTEM_NAME}")
message(STATUS "    - Version                   ${CMAKE_SYSTEM_VERSION}")
message(STATUS "    - Processor                 ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  Build")
message(STATUS "    - Compiler                  ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "    - Extra compilation flags   ${CMAKE_CXX_FLAGS}")
message(STATUS "    - Build unit tests          ${UNIT_TEST}")
message(STATUS "")
message(STATUS "  Installation")
message(STATUS "    - Prefix                    ${PREFIX}")
message(STATUS "    - Library directory         ${PREFIX_LIB}")
message(STATUS "    - Include directory         ${PREFIX_INC}")
message(STATUS "    - Package                   ${PACKAGE_LIST}")
if (WITH_PKGCONFIG_SCRIPT)
  message(STATUS "    - pkg-config directory       ${PKGCONFIG_DIR}")
endif ()
message(STATUS "")
