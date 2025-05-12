#
# Copyright 2011-2024 Centreon
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
project("Centreon Engine" C CXX)

# set -latomic if OS is Raspbian.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

# With libasan
option(WITH_ASAN
       "Add the libasan to check memory leaks and other memory issues." OFF)

if(WITH_ASAN)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS_DEBUG
      "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

set(INC_DIR "${PROJECT_SOURCE_DIR}/inc")
set(SCRIPT_DIR "${PROJECT_SOURCE_DIR}/scripts")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(PRECOMP_HEADER "${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh")

set(ENGINE_MODULES_DIR "${CMAKE_INSTALL_PREFIX}/lib64/centreon-engine/")

include_directories("${INC_DIR}" "${INC_DIR}/compatibility"
                    "${CMAKE_SOURCE_DIR}/common/inc")

link_directories(${CMAKE_SOURCE_DIR}/build/centreon-clib/)

# Version.
if(CENTREON_ENGINE_PRERELEASE)
  set(CENTREON_ENGINE_VERSION
      "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}-${CENTREON_ENGINE_PRERELEASE}"
  )
else()
  set(CENTREON_ENGINE_VERSION
      "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")
endif()

message(STATUS "Generating version header (${CENTREON_ENGINE_VERSION}).")
configure_file("${INC_DIR}/com/centreon/engine/version.hh.in"
               "${INC_DIR}/com/centreon/engine/version.hh")

#
# Check and/or find required components.
#

# Check libraries to link with.
include(CheckLibraryExists)
message(STATUS "Checking for libm.")
check_library_exists("m" "ceil" "${CMAKE_LIBRARY_PATH}" MATH_LIB_FOUND)

if(MATH_LIB_FOUND)
  set(MATH_LIBRARIES "m")
endif()

message(STATUS "Checking for libnsl.")
check_library_exists("nsl" "getservbyname" "${CMAKE_LIBRARY_PATH}"
                     NSL_LIB_FOUND)

if(NSL_LIB_FOUND)
  set(NSL_LIBRARIES "nsl")
endif()

message(STATUS "Checking for libsocket.")
check_library_exists("socket" "connect" "${CMAKE_LIBRARY_PATH}"
                     SOCKET_LIB_FOUND)

if(SOCKET_LIB_FOUND)
  set(SOCKET_LIBRARIES "socket")
endif()

# Find pthreads.
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
include(FindThreads)

if(NOT CMAKE_USE_PTHREADS_INIT)
  message(FATAL_ERROR "Could not find pthread's library.")
endif()

set(PTHREAD_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")

include_directories(${CMAKE_SOURCE_DIR}/clib/inc)

# Check functions.
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckStructHasMember)

message(STATUS "Checking for tm_zone member in tm struct.")
check_struct_has_member("tm" "tm_zone" "time.h" HAVE_TM_ZONE)

if(HAVE_TM_ZONE)
  add_definitions(-DHAVE_TM_ZONE)
endif()

include(CheckSymbolExists)
message(STATUS "Checking for symbol tzname.")
check_symbol_exists("tzname" "time.h" HAVE_TZNAME)

if(HAVE_TZNAME)
  add_definitions(-DHAVE_TZNAME)
endif()

message(STATUS "Checking for function getopt_long.")
check_include_file_cxx("getopt.h" HAVE_GETOPT_H)
check_function_exists("getopt_long" HAVE_GETOPT_LONG)

if(HAVE_GETOPT_H AND HAVE_GETOPT_LONG)
  add_definitions(-DHAVE_GETOPT_H)
endif()

#
# Options.
#

# Enable or disable the OFFLINE BUILD mode.
option(BUILD_OFFLINE "Build offline" OFF)

# Enable or disable installation.
option(WITH_CREATE_FILES "Create centreon-engine files." ON)
set(CREATE_FILES "${WITH_CREATE_FILES}")

set(PREFIX_ENGINE_CONF "${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-engine")

#

# User used to run Centreon Engine.
if(WITH_USER_ENGINE)
  set(USER "${WITH_USER_ENGINE}")
else()
  set(USER "root")
endif()

# Group used to run Centreon Engine.
if(WITH_GROUP_ENGINE)
  set(GROUP "${WITH_GROUP_ENGINE}")
else()
  set(GROUP "root")
endif()

# Set startup script to auto if not define.
if(NOT WITH_STARTUP_SCRIPT)
  set(WITH_STARTUP_SCRIPT "auto")
endif()

# Check which startup script to use.
if(WITH_STARTUP_SCRIPT STREQUAL "auto")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(OS_DISTRIBUTOR STREQUAL "Ubuntu")
      set(WITH_STARTUP_SCRIPT "upstart")
    else()
      set(WITH_STARTUP_SCRIPT "sysv")
    endif()
  else()
    message(
      STATUS
        "Centreon Engine does not provide startup script for ${CMAKE_SYSTEM_NAME}."
    )
  endif()
endif()

# Create upstart file.
if(WITH_STARTUP_SCRIPT STREQUAL "upstart")
  # Generate Upstart script.
  message(STATUS "Generating upstart script.")
  configure_file("${SCRIPT_DIR}/upstart.conf.in" "${SCRIPT_DIR}/upstart.conf")

  # Startup dir.
  if(WITH_STARTUP_DIR)
    set(STARTUP_DIR "${WITH_STARTUP_DIR}")
  else()
    set(STARTUP_DIR "/etc/init")
  endif()

  # Script install rule.
  install(
    FILES "${SCRIPT_DIR}/upstart.conf"
    DESTINATION "${STARTUP_DIR}"
    COMPONENT "runtime"
    RENAME "centengine.conf")

  # String printed in summary.
  set(STARTUP_SCRIPT "Upstart configuration file")

  # Create SysV start script.
elseif(WITH_STARTUP_SCRIPT STREQUAL "sysv")
  # Lock file.
  if(WITH_LOCK_FILE)
    set(LOCK_FILE "${WITH_LOCK_FILE}")
  else()
    if(OS_DISTRIBUTOR STREQUAL "Ubuntu"
       OR OS_DISTRIBUTOR STREQUAL "Debian"
       OR OS_DISTRIBUTOR STREQUAL "SUSE LINUX")
      set(LOCK_FILE "/var/lock/centengine.lock")
    else()
      set(LOCK_FILE "/var/lock/subsys/centengine.lock")
    endif()
  endif()

  string(REGEX REPLACE "/[^/]*$" "" LOCK_DIR "${LOCK_FILE}")

  # PID file.
  if(WITH_PID_FILE)
    set(PID_FILE "${WITH_PID_FILE}")
  else()
    set(PID_FILE "/var/run/centengine.pid")
  endif()

  string(REGEX REPLACE "/[^/]*$" "" PID_DIR "${PID_FILE}")

  # Generate SysV script.
  message(STATUS "Generating generic startup script.")
  configure_file("${SCRIPT_DIR}/centengine.sh.in" "${SCRIPT_DIR}/centengine.sh")

  # Startup dir.
  if(WITH_STARTUP_DIR)
    set(STARTUP_DIR "${WITH_STARTUP_DIR}")
  else()
    set(STARTUP_DIR "/etc/init.d")
  endif()

  # Script install rule.
  install(
    PROGRAMS "${SCRIPT_DIR}/centengine.sh"
    DESTINATION "${STARTUP_DIR}"
    COMPONENT "runtime"
    RENAME "centengine")

  # String printed in summary.
  set(STARTUP_SCRIPT "SysV-style script")

  # Create Systemd start script.
elseif(WITH_STARTUP_SCRIPT STREQUAL "systemd")
  # Generate Systemd script.
  message(STATUS "Generating systemd startup script.")
  configure_file("${SCRIPT_DIR}/centengine.service.in"
                 "${SCRIPT_DIR}/centengine.service")

  # Startup dir.
  if(WITH_STARTUP_DIR)
    set(STARTUP_DIR "${WITH_STARTUP_DIR}")
  else()
    set(STARTUP_DIR "/etc/systemd/system")
  endif()

  # Script install rule.
  install(
    PROGRAMS "${SCRIPT_DIR}/centengine.service"
    DESTINATION "${STARTUP_DIR}"
    COMPONENT "runtime")

  # String printed in summary.
  set(STARTUP_SCRIPT "Systemd script")

else()
  # Default.
  message(
    STATUS
      "Invalid value for option WITH_STARTUP_SCRIPT (must be one of 'auto', 'sysv' or 'upstart')."
  )
  set(STARTUP_SCRIPT "disabled")
endif()

# logrotate directory.
option(WITH_ENGINE_LOGROTATE_SCRIPT "Generate and install logrotate script."
       OFF)

if(WITH_ENGINE_LOGROTATE_SCRIPT)
  # Generate logrotate file.
  message(STATUS "Generating logrorate file.")

  if(WITH_STARTUP_SCRIPT STREQUAL "upstart")
    configure_file("${SCRIPT_DIR}/logrotate_upstart.conf.in"
                   "${SCRIPT_DIR}/logrotate.conf" @ONLY)
  elseif(WITH_STARTUP_SCRIPT STREQUAL "systemd")
    configure_file("${SCRIPT_DIR}/logrotate_systemd.conf.in"
                   "${SCRIPT_DIR}/logrotate.conf" @ONLY)
  else()
    configure_file("${SCRIPT_DIR}/logrotate_sysv.conf.in"
                   "${SCRIPT_DIR}/logrotate.conf" @ONLY)
  endif()

  # logrotate file install directory.
  set(LOGROTATE_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/logrotate.d")

  # Install rule.
  install(
    FILES "${SCRIPT_DIR}/logrotate.conf"
    DESTINATION "${LOGROTATE_DIR}"
    COMPONENT "runtime"
    RENAME "centengine")
endif()

option(
  WITH_SHARED_LIB
  "Define if the core library is to be build as a shared object or a static library."
  OFF)

if(WITH_SHARED_LIB)
  set(LIBRARY_TYPE SHARED)
else()
  set(LIBRARY_TYPE STATIC)
endif()

# Simumod module to simulate cbmod and catch its output
option(WITH_SIMU
       "Add a module only used for tests to see data that cbmod should receive"
       OFF)

if(WITH_SIMU)
  set(CMAKE_BUILD_TYPE "Debug")
  add_subdirectory(src/simumod)
endif()

# DEBUG_CONFIG enables checks on configuration. Those checks are not free and
# may slow down engine reloads. But it provides a way to check bugs in the
# configuration system.
option(WITH_DEBUG_CONFIG
       "Enables checks on configuration. This is an option for developers." OFF)

if(WITH_DEBUG_CONFIG)
  add_definitions(-DDEBUG_CONFIG)
endif()

# Configure files.
configure_file("${INC_DIR}/compatibility/common.h.in"
               "${INC_DIR}/compatibility/common.h")

# Locations definitions
add_definitions(-DDEFAULT_LOG_ARCHIVE_PATH="${ENGINE_VAR_LOG_ARCHIVE_DIR}")
add_definitions(-DDEFAULT_CONFIG_FILE="${PREFIX_ENGINE_CONF}/centengine.cfg")
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# Add specific linker flags for Mac OS to build correctly shared libraries.
if(APPLE)
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-undefined -Wl,dynamic_lookup")
endif()

#
# Targets.
#
set(FILES
    # Sources.
    "${SRC_DIR}/anomalydetection.cc"
    "${SRC_DIR}/broker.cc"
    "${SRC_DIR}/checkable.cc"
    "${SRC_DIR}/check_result.cc"
    "${SRC_DIR}/command_manager.cc"
    "${SRC_DIR}/comment.cc"
    "${SRC_DIR}/config.cc"
    "${SRC_DIR}/contact.cc"
    "${SRC_DIR}/contactgroup.cc"
    "${SRC_DIR}/customvariable.cc"
    "${SRC_DIR}/daterange.cc"
    "${SRC_DIR}/dependency.cc"
    "${SRC_DIR}/diagnostic.cc"
    "${SRC_DIR}/exceptions/error.cc"
    "${SRC_DIR}/flapping.cc"
    "${SRC_DIR}/escalation.cc"
    "${SRC_DIR}/globals.cc"
    "${SRC_DIR}/host.cc"
    "${SRC_DIR}/hostdependency.cc"
    "${SRC_DIR}/hostescalation.cc"
    "${SRC_DIR}/hostgroup.cc"
    "${SRC_DIR}/macros.cc"
    "${SRC_DIR}/nebmods.cc"
    "${SRC_DIR}/notification.cc"
    "${SRC_DIR}/notifier.cc"
    "${SRC_DIR}/sehandlers.cc"
    "${SRC_DIR}/service.cc"
    "${SRC_DIR}/servicedependency.cc"
    "${SRC_DIR}/serviceescalation.cc"
    "${SRC_DIR}/servicegroup.cc"
    "${SRC_DIR}/severity.cc"
    "${SRC_DIR}/shared.cc"
    "${SRC_DIR}/statistics.cc"
    "${SRC_DIR}/statusdata.cc"
    "${SRC_DIR}/string.cc"
    "${SRC_DIR}/tag.cc"
    "${SRC_DIR}/timeperiod.cc"
    "${SRC_DIR}/timerange.cc"
    "${SRC_DIR}/timezone_locker.cc"
    "${SRC_DIR}/timezone_manager.cc"
    "${SRC_DIR}/utils.cc"
    "${SRC_DIR}/xsddefault.cc"
    # Headers.
    "${INC_DIR}/com/centreon/engine/anomalydetection.hh"
    "${INC_DIR}/com/centreon/engine/broker.hh"
    "${INC_DIR}/com/centreon/engine/checkable.hh"
    "${INC_DIR}/com/centreon/engine/check_result.hh"
    "${INC_DIR}/com/centreon/engine/circular_buffer.hh"
    "${INC_DIR}/com/centreon/engine/command_manager.hh"
    "${INC_DIR}/com/centreon/engine/comment.hh"
    "${INC_DIR}/com/centreon/engine/common.hh"
    "${INC_DIR}/com/centreon/engine/config.hh"
    "${INC_DIR}/com/centreon/engine/contact.hh"
    "${INC_DIR}/com/centreon/engine/contactgroup.hh"
    "${INC_DIR}/com/centreon/engine/customvariable.hh"
    "${INC_DIR}/com/centreon/engine/daterange.hh"
    "${INC_DIR}/com/centreon/engine/dependency.hh"
    "${INC_DIR}/com/centreon/engine/diagnostic.hh"
    "${INC_DIR}/com/centreon/engine/exceptions/error.hh"
    "${INC_DIR}/com/centreon/engine/escalation.hh"
    "${INC_DIR}/com/centreon/engine/flapping.hh"
    "${INC_DIR}/com/centreon/engine/globals.hh"
    "${INC_DIR}/com/centreon/engine/host.hh"
    "${INC_DIR}/com/centreon/engine/hostdependency.hh"
    "${INC_DIR}/com/centreon/engine/hostescalation.hh"
    "${INC_DIR}/com/centreon/engine/hostgroup.hh"
    "${INC_DIR}/com/centreon/engine/logging.hh"
    "${INC_DIR}/com/centreon/engine/macros.hh"
    "${INC_DIR}/com/centreon/engine/nebcallbacks.hh"
    "${INC_DIR}/com/centreon/engine/neberrors.hh"
    "${INC_DIR}/com/centreon/engine/nebmods.hh"
    "${INC_DIR}/com/centreon/engine/nebmodules.hh"
    "${INC_DIR}/com/centreon/engine/nebstructs.hh"
    "${INC_DIR}/com/centreon/engine/notification.hh"
    "${INC_DIR}/com/centreon/engine/notifier.hh"
    "${INC_DIR}/com/centreon/engine/objects.hh"
    "${INC_DIR}/com/centreon/engine/sehandlers.hh"
    "${INC_DIR}/com/centreon/engine/service.hh"
    "${INC_DIR}/com/centreon/engine/servicedependency.hh"
    "${INC_DIR}/com/centreon/engine/serviceescalation.hh"
    "${INC_DIR}/com/centreon/engine/servicegroup.hh"
    "${INC_DIR}/com/centreon/engine/severity.hh"
    "${INC_DIR}/com/centreon/engine/shared.hh"
    "${INC_DIR}/com/centreon/engine/statistics.hh"
    "${INC_DIR}/com/centreon/engine/statusdata.hh"
    "${INC_DIR}/com/centreon/engine/string.hh"
    "${INC_DIR}/com/centreon/engine/tag.hh"
    "${INC_DIR}/com/centreon/engine/timeperiod.hh"
    "${INC_DIR}/com/centreon/engine/timerange.hh"
    "${INC_DIR}/com/centreon/engine/timezone_locker.hh"
    "${INC_DIR}/com/centreon/engine/timezone_manager.hh"
    "${INC_DIR}/com/centreon/engine/utils.hh"
    "${INC_DIR}/com/centreon/engine/version.hh"
    "${INC_DIR}/com/centreon/engine/xsddefault.hh")

# Subdirectories with core features.
add_subdirectory(src/broker)
add_subdirectory(src/checks)

if(WITH_CONF)
  add_subdirectory(conf)
endif()

add_subdirectory(src/downtimes)
add_subdirectory(src/configuration)
add_subdirectory(src/commands)
add_subdirectory(src/compatibility)
add_subdirectory(src/deleter)
add_subdirectory(src/events)
add_subdirectory(src/logging)
add_subdirectory(src/macros)
add_subdirectory(modules)
add_subdirectory(src/retention)
add_subdirectory(enginerpc)
include_directories(enginerpc ${CMAKE_SOURCE_DIR}/common/src
                    ${CMAKE_SOURCE_DIR}/common/inc)

# centenginestats target.
add_executable(centenginestats "${SRC_DIR}/centenginestats.cc")
add_dependencies(centenginestats centreon_clib)
target_link_libraries(centenginestats centreon_clib fmt::fmt)
target_precompile_headers(centenginestats PRIVATE ${PRECOMP_HEADER})

# Library engine target.

if(LEGACY_ENGINE)
  add_library(cce_core ${LIBRARY_TYPE} ${FILES})
  add_dependencies(cce_core engine_rpc centreon_clib pb_neb_lib)

  target_precompile_headers(cce_core PRIVATE ${PRECOMP_HEADER})

  # Link target with required libraries.
  target_link_libraries(
    cce_core
    ${MATH_LIBRARIES}
    ${PTHREAD_LIBRARIES}
    ${SOCKET_LIBRARIES}
    centreon_clib
    engine_legacy_conf
    fmt::fmt
    spdlog::spdlog)

  # centengine target.

  # centengine target.
  add_executable(centengine "${SRC_DIR}/main.cc")
  set_property(TARGET centengine PROPERTY ENABLE_EXPORTS "1")
  add_dependencies(centengine centreon_clib)

  # Link centengine with required libraries.
  target_link_libraries(
    centengine
    -L${PROTOBUF_LIB_DIR}
    "-rdynamic"
    centreon_clib
    log_v2
    "-Wl,-whole-archive"
    enginerpc_legacy
    centreon_grpc
    centreon_http
    centreon_common
    boost_url
    cce_core
    gRPC::grpc++
    Boost::program_options
    "-Wl,--no-whole-archive"
    gRPC::gpr
    gRPC::grpc
    gRPC::grpc++_alts
    absl::any
    absl::log
    absl::base
    absl::bits
    crypto
    ssl
    ${c-ares_LIBS}
    z
    ryml::ryml
    stdc++fs
    dl)

install(
  TARGETS centengine centenginestats
  DESTINATION "${CMAKE_INSTALL_FULL_SBINDIR}"
  COMPONENT "runtime")
else()
  add_library(cce_core ${LIBRARY_TYPE} ${FILES})
  add_dependencies(cce_core engine_rpc centreon_clib pb_neb_lib)

  target_precompile_headers(cce_core PRIVATE ${PRECOMP_HEADER})

  # Link target with required libraries.
  target_link_libraries(
    cce_core
    ${MATH_LIBRARIES}
    ${PTHREAD_LIBRARIES}
    ${SOCKET_LIBRARIES}
    centreon_clib
    engine_conf
    fmt::fmt
    spdlog::spdlog)

  # centengine target.

  add_executable(centengine "${SRC_DIR}/main.cc")
  set_property(TARGET centengine PROPERTY ENABLE_EXPORTS "1")
  add_dependencies(centengine centreon_clib)

  # Link centengine with required libraries.
  target_link_libraries(
    centengine
    -L${PROTOBUF_LIB_DIR}
    "-rdynamic"
    centreon_clib
    log_v2
    "-Wl,-whole-archive"
    enginerpc
    centreon_grpc
    centreon_http
    centreon_common
    boost_url
    cce_core
    gRPC::grpc++
    Boost::program_options
    protobuf
    "-Wl,--no-whole-archive"
    gRPC::gpr
    gRPC::grpc
    gRPC::grpc++_alts
    absl::any
    absl::log
    absl::base
    absl::bits
    crypto
    ssl
    ${c-ares_LIBS}
    z
    ryml::ryml
    stdc++fs
    dl)

install(
  TARGETS centengine centenginestats
  DESTINATION "${CMAKE_INSTALL_FULL_SBINDIR}"
  COMPONENT "runtime")
endif()

# Unit tests.
if (NOT ONLY_ROBOT_CMA)
  add_subdirectory(tests)
endif()

#
# Install stuff.
#

# Install rules.

# Create directories.
if(CREATE_FILES)
  install(
    CODE "
  function(mkdir_chown user group path)
    if (APPLE OR (UNIX AND NOT CYGWIN))
      if (NOT EXISTS \"\$ENV{DESTDIR}\${path}\")
        file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${path}\")
        execute_process(COMMAND \"chown\" \"\${user}:\${group}\" \"\$ENV{DESTDIR}\${path}\")
      endif ()
    else()
      file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${path}\")
    endif ()
  endfunction()

  function(touch_chown user group file)
    if (APPLE OR (UNIX AND NOT CYGWIN))
      if (NOT EXISTS \"\$ENV{DESTDIR}\${file}\")
        file(WRITE \"\$ENV{DESTDIR}\${file}\" \"\")
        execute_process(COMMAND \"chown\" \"\${user}:\${group}\" \"\$ENV{DESTDIR}\${file}\")
      endif ()
    else()
      file(WRITE \"\$ENV{DESTDIR}\${file}\" \"\")
    endif ()
  endfunction()

  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${PREFIX_ENGINE_CONF}\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${PREFIX_ENGINE_CONF}/objects\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${ENGINE_VAR_LOG_DIR}\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${ENGINE_VAR_LOG_ARCHIVE_DIR}\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${ENGINE_VAR_LIB_DIR}/rw\")
  if (LOCK_DIR)
    mkdir_chown(\"${USER}\" \"${GROUP}\" \"${LOCK_DIR}\")
  endif ()
  if (PID_DIR)
    mkdir_chown(\"${USER}\" \"${GROUP}\" \"${PID_DIR}\")
  endif ()

  touch_chown(\"${USER}\" \"${GROUP}\" \"${ENGINE_VAR_LOG_DIR}/status.dat\")
  touch_chown(\"${USER}\" \"${GROUP}\" \"${ENGINE_VAR_LOG_DIR}/centengine.log\")
  touch_chown(\"${USER}\" \"${GROUP}\" \"${ENGINE_VAR_LOG_DIR}/centengine.debug\")
  touch_chown(\"${USER}\" \"${GROUP}\" \"${ENGINE_VAR_LOG_DIR}/retention.dat\")
  ")
endif()

# Install header files for development.
install(
  DIRECTORY "${INC_DIR}/"
  DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}/centreon-engine/"
  COMPONENT "development"
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "${INC_DIR}/compatibility/" EXCLUDE)

#
# Packaging.
#
include(cmake/package.cmake)

#
# Print summary.
#
message(STATUS "")
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  Project")
message(STATUS "    - Name                        Centreon Engine")
message(STATUS "    - Version                     ${CENTREON_ENGINE_VERSION}")
message(STATUS "")
message(STATUS "  System")
message(STATUS "    - Name                        ${CMAKE_SYSTEM_NAME}")
message(STATUS "    - Version                     ${CMAKE_SYSTEM_VERSION}")
message(STATUS "    - Processor                   ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  Build")
message(
  STATUS
    "    - Compiler                    ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})"
)
message(STATUS "    - Extra compilation flags     ${CMAKE_CXX_FLAGS}")

if(WITH_SHARED_LIB)
  message(STATUS "    - Build static core library   no")
else()
  message(STATUS "    - Build static core library   yes")
endif()

message(STATUS "    - External commands module    enabled")

if(WITH_TESTING)
  message(STATUS "    - Unit tests                  enabled")

  if(WITH_COVERAGE)
    message(STATUS "    - Code coverage               enabled")
  else()
    message(STATUS "    - Code coverage               disabled")
  endif()
else()
  message(STATUS "    - Unit tests                  disabled")
endif()

if(WITH_ENGINE_LOGROTATE_SCRIPT)
  message(STATUS "    - logrotate script            enabled")
else()
  message(STATUS "    - logrotate script            disabled")
endif()

message(STATUS "    - Startup script              ${STARTUP_SCRIPT}")
message(STATUS "")
message(STATUS "  Install")

# message(STATUS "    - Prefix                      ${CMAKE_INSTALL_PREFIX}")
message(
  STATUS "    - Binary prefix               ${CMAKE_INSTALL_FULL_SBINDIR}")
message(STATUS "    - Configuration prefix        ${PREFIX_ENGINE_CONF}")

# message(STATUS "    - Library prefix              ${PREFIX_LIB}")
message(
  STATUS
    "    - Include prefix              ${CMAKE_INSTALL_FULL_INCLUDEDIR}/centreon-engine"
)
message(STATUS "    - var directory               ${VAR_DIR}")
message(
  STATUS "    - Log archive directory       ${ENGINE_VAR_LOG_ARCHIVE_DIR}")
message(STATUS "    - RW directory                ${ENGINE_VAR_LIB_DIR}/rw")

if(LOCK_FILE)
  message(STATUS "    - Lock prefix                 ${LOCK_FILE}")
endif()

if(WITH_ENGINE_LOGROTATE_SCRIPT)
  message(STATUS "    - logrotate directory         ${LOGROTATE_DIR}")
endif()

if(STARTUP_DIR)
  message(STATUS "    - Startup directory           ${STARTUP_DIR}")
endif()

message(STATUS "    - User                        ${USER}")
message(STATUS "    - Group                       ${GROUP}")
message(STATUS "    - Package                     ${PACKAGE_LIST}")
message(STATUS "")
message(STATUS "  Libraries")
message(STATUS "    - clib include directory      ${CLIB_INCLUDE_DIR}")
message(STATUS "    - clib library directory      centreon_clib")
