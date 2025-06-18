#
# Copyright 2009-2025 Centreon
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
project("Centreon Broker" C CXX)

# set -latomic if OS is Raspbian.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")

option(WITH_LIBCXX "compiles and link cbd with clang++/libc++")

if(WITH_LIBCXX)
  set(CMAKE_CXX_COMPILER "clang++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -Werror -O1
  # -fno-omit-frame-pointer")
endif()

# With ASIO DEBUGGING ENABLED
option(WITH_DEBUG_ASIO "Add the Asio debugging flags." OFF)

if(WITH_DEBUG_ASIO)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS_DEBUG
      "${CMAKE_CXX_FLAGS_DEBUG} -DBOOST_ASIO_ENABLE_BUFFER_DEBUGGING -DBOOST_ASIO_ENABLE_HANDLER_TRACKING"
  )
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

# Default MySQL socket
if(OS_DISTRIBUTOR STREQUAL "Debian" OR OS_DISTRIBUTOR STREQUAL "Ubuntu")
  message(STATUS "deb based os")
  add_definitions("-DMYSQL_SOCKET=\"/var/run/mysqld/mysqld.sock\"")
  add_definitions("-DDEFAULT_MARIADB_EXTENSION_DIR=\"/usr/lib/x86_64-linux-gnu/libmariadb3/plugin\"")
elseif(OS_DISTRIBUTOR STREQUAL "CentOS" OR OS_DISTRIBUTOR STREQUAL "RedHat")
  message(STATUS "rpm based os")
  add_definitions("-DMYSQL_SOCKET=\"/var/lib/mysql/mysql.sock\"")
  add_definitions("-DDEFAULT_MARIADB_EXTENSION_DIR=\"/usr/lib64/mariadb/plugin\"")
else()
  message(STATUS "other os: ${OS_DISTRIBUTOR}")
  add_definitions("-DMYSQL_SOCKET=\"/tmp/mysql.sock\"")
  add_definitions("-DDEFAULT_MARIADB_EXTENSION_DIR=\"/usr/lib/x86_64-linux-gnu/libmariadb3/plugin\"")
endif()

include_directories(
  ${CMAKE_BINARY_DIR}/bbdo
  ${CMAKE_BINARY_DIR}/common/process_stat
  "${PROJECT_SOURCE_DIR}/core/inc"
  "${PROJECT_SOURCE_DIR}/neb/inc"
  "${CMAKE_SOURCE_DIR}/engine/inc"
  "${PROJECT_SOURCE_DIR}/core/multiplexing/inc"
  "${PROJECT_SOURCE_DIR}/core/sql/inc"
  ${CMAKE_BINARY_DIR}/common/engine_conf
  "${MARIADB_INCLUDE_DIRS}")
set(INC_DIR "${PROJECT_SOURCE_DIR}/core/inc/com/centreon/broker")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/core/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/core/test")

add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
add_definitions(${spdlog_DEFINITIONS} ${mariadb-connector-c_DEFINITIONS})

# link_directories(${mariadb-connector-c_LIB_DIRS})
add_custom_command(
  DEPENDS ${SRC_DIR}/broker.proto
  COMMENT "Generating interface files of the proto file (grpc)"
  OUTPUT ${SRC_DIR}/broker.grpc.pb.cc ${SRC_DIR}/broker.grpc.pb.h
  COMMAND
    protobuf::protoc ARGS
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN} --proto_path=${SRC_DIR}
    --proto_path=${CMAKE_SOURCE_DIR}/bbdo
    --proto_path=${CMAKE_SOURCE_DIR}/common/process_stat --grpc_out="${SRC_DIR}"
    ${SRC_DIR}/broker.proto
  DEPENDS ${SRC_DIR}/broker.proto
  COMMENT "Generating interface files of the proto file (protobuf)"
  OUTPUT ${SRC_DIR}/broker.pb.cc ${SRC_DIR}/broker.pb.h
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out="${SRC_DIR}"
    --proto_path=${CMAKE_SOURCE_DIR}/bbdo
    --proto_path=${SRC_DIR} --proto_path=${CMAKE_SOURCE_DIR}/common/process_stat
    ${SRC_DIR}/broker.proto
    WORKING_DIRECTORY ${SRC_DIR})

add_custom_target(target_broker_message DEPENDS "${SRC_DIR}/broker.pb.cc"
                                                  "${SRC_DIR}/broker.pb.h")

include_directories(${SRC_DIR} ${CMAKE_SOURCE_DIR}/common/src
                    ${CMAKE_SOURCE_DIR}/common/inc
                    ${CMAKE_SOURCE_DIR}/bbdo)

# Version.
if(CENTREON_BROKER_PRERELEASE)
  set(CENTREON_BROKER_VERSION
      "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}-${CENTREON_BROKER_PRERELEASE}"
  )
else()
  set(CENTREON_BROKER_VERSION
      "${COLLECT_MAJOR}.${COLLECT_MINOR}.${COLLECT_PATCH}")
endif()

add_definitions(-DCENTREON_BROKER_VERSION=\"${CENTREON_BROKER_VERSION}\"
                -DCENTREON_BROKER_PATCH=${COLLECT_PATCH})
configure_file(${INC_DIR}/version.hh.in ${INC_DIR}/version.hh @ONLY)

include(cmake/tool.cmake)

#
# Check and/or find required components.
#
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

#
# Options.
#
set(PREFIX_BROKER_CONF ${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-broker)

# Modules directories.
set(PREFIX_MODULES "${CMAKE_INSTALL_FULL_DATADIR}/centreon/lib/centreon-broker")
set(PREFIX_CBMOD "${LIBDIR}")

# User.
if(WITH_USER_BROKER)
  set(USER "${WITH_USER_BROKER}")
else()
  set(USER "root")
endif()

# Group.
if(WITH_GROUP_BROKER)
  set(GROUP "${WITH_GROUP_BROKER}")
else()
  set(GROUP "root")
endif()

# Set startup script to auto if not defined.
if(NOT WITH_STARTUP_SCRIPT)
  set(WITH_STARTUP_SCRIPT "auto")
endif()

# Check which startup script to use.
if(WITH_STARTUP_SCRIPT STREQUAL "auto")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(WITH_STARTUP_SCRIPT "sysv")
  else()
    message(
      STATUS
        "Centreon Broker does not provide startup script for ${CMAKE_SYSTEM_NAME}."
    )
    set(WITH_STARTUP_SCRIPT "no")
  endif()
endif()

# Startup dir.
if(WITH_STARTUP_SCRIPT STREQUAL "sysv" OR WITH_STARTUP_SCRIPT STREQUAL
                                          "systemd")
  # Set destination directory.
  if(WITH_STARTUP_DIR)
    set(STARTUP_DIR "${WITH_STARTUP_DIR}")
  else()
    set(STARTUP_DIR "/etc/init.d")
  endif()
endif()

# Configure files.
if(WITH_DAEMONS)
  if(WITH_STARTUP_SCRIPT STREQUAL "systemd")
    configure_file("${PROJECT_SOURCE_DIR}/script/systemd/cbd.service.in"
                   "${PROJECT_SOURCE_DIR}/script/cbd.service" @ONLY)
    install(FILES "${PROJECT_SOURCE_DIR}/script/cbd.service"
            DESTINATION "${STARTUP_DIR}")
  elseif(OS_DISTRIBUTOR STREQUAL "CentOS" OR OS_DISTRIBUTOR STREQUAL "RedHat")
    configure_file("${PROJECT_SOURCE_DIR}/script/redhat/cbd.init.d.in"
                   "${PROJECT_SOURCE_DIR}/script/cbd.init" @ONLY)
  elseif(OS_DISTRIBUTOR STREQUAL "Debian" OR OS_DISTRIBUTOR STREQUAL "Ubuntu")
    configure_file("${PROJECT_SOURCE_DIR}/script/debian/cbd.init.d.in"
                   "${PROJECT_SOURCE_DIR}/script/cbd.init" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/script/debian/cbd.default.in"
                   "${PROJECT_SOURCE_DIR}/script/cbd.default" @ONLY)
    install(
      FILES "${PROJECT_SOURCE_DIR}/script/cbd.default"
      DESTINATION "/etc/default"
      RENAME "cbd")
  else()
    configure_file("${PROJECT_SOURCE_DIR}/script/other/cbd.init.d.in"
                   "${PROJECT_SOURCE_DIR}/script/cbd.init" @ONLY)
  endif()

  configure_file("${PROJECT_SOURCE_DIR}/script/watchdog.json.in"
                 "${PROJECT_SOURCE_DIR}/script/watchdog.json" @ONLY)
  install(FILES "${PROJECT_SOURCE_DIR}/script/watchdog.json"
          DESTINATION "${PREFIX_BROKER_CONF}")

  if(WITH_STARTUP_SCRIPT STREQUAL "sysv")
    install(
      PROGRAMS "${PROJECT_SOURCE_DIR}/script/cbd.init"
      DESTINATION "${STARTUP_DIR}"
      RENAME "cbd")
  endif()
endif()

if(WITH_CONF)
  configure_file("${PROJECT_SOURCE_DIR}/config/central-module.json.in"
                 "${PROJECT_SOURCE_DIR}/config/central-module.json" @ONLY)
  configure_file("${PROJECT_SOURCE_DIR}/config/central-broker.json.in"
                 "${PROJECT_SOURCE_DIR}/config/central-broker.json" @ONLY)
  configure_file("${PROJECT_SOURCE_DIR}/config/central-rrd.json.in"
                 "${PROJECT_SOURCE_DIR}/config/central-rrd.json" @ONLY)

  install(FILES "${PROJECT_SOURCE_DIR}/config/central-module.json"
          DESTINATION "${PREFIX_BROKER_CONF}")
  install(FILES "${PROJECT_SOURCE_DIR}/config/central-rrd.json"
          DESTINATION "${PREFIX_BROKER_CONF}")
  install(FILES "${PROJECT_SOURCE_DIR}/config/central-broker.json"
          DESTINATION "${PREFIX_BROKER_CONF}")
endif()

# Monitoring engine (for testing).
if(WITH_MONITORING_ENGINE)
  set(MONITORING_ENGINE_ADDITIONAL "")
  set(MONITORING_ENGINE "${WITH_MONITORING_ENGINE}")

  if(WITH_MONITORING_ENGINE_MODULES)
    foreach(MODULE IN LISTS WITH_MONITORING_ENGINE_MODULES)
      set(MONITORING_ENGINE_ADDITIONAL
          "${MONITORING_ENGINE_ADDITIONAL}broker_module=${MODULE}\\n")
    endforeach()
  endif()

  if(WITH_MONITORING_ENGINE_INTERVAL_LENGTH)
    set(MONITORING_ENGINE_INTERVAL_LENGTH
        "${WITH_MONITORING_ENGINE_INTERVAL_LENGTH}")
  else()
    set(MONITORING_ENGINE_INTERVAL_LENGTH 1)
  endif()
endif()

set(LUA_FOUND FALSE)

# Lua module.
option(WITH_MODULE_LUA "Build lua module." ON)

# Simu module.
option(WITH_MODULE_SIMU "Build simu module." OFF)

if(WITH_MODULE_LUA OR WITH_MODULE_SIMU)
  foreach(vers "Lua" "Lua52" "Lua51")
    find_package("${vers}")

    if(DEFINED LUA_VERSION_STRING)
      string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.[0-9]+" "\\1\\2" v
                           "${LUA_VERSION_STRING}")
      add_definitions(-DLUA${v})
      include_directories("${LUA_INCLUDE_DIR}")

      set(LUA_FOUND TRUE)
      break()
    endif()
  endforeach()

  if(NOT DEFINED LUA_VERSION_STRING)
    message(FATAL_ERROR "No Lua development package found.")
  endif()
endif()

# Broker vars
configure_file("${INC_DIR}/vars.hh.in" "${INC_DIR}/vars.hh" "@ONLY")

add_subdirectory(core/multiplexing)

add_library(pb_broker_lib STATIC
    ${SRC_DIR}/broker.grpc.pb.cc
    ${SRC_DIR}/broker.grpc.pb.h
    ${SRC_DIR}/broker.pb.cc
    ${SRC_DIR}/broker.pb.h
  )

add_dependencies(pb_broker_lib process_stat pb_common_lib)

target_link_libraries(pb_broker_lib
  PRIVATE
  pb_common_lib
  process_stat
)

set_target_properties(pb_broker_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(core/bbdo)

# Core library.
set(LIBROKER_SOURCES
    # Sources.
    ${SRC_DIR}/broker_impl.cc
    ${SRC_DIR}/brokerrpc.cc
    ${SRC_DIR}/cache/global_cache.cc
    ${SRC_DIR}/cache/global_cache_data.cc
    ${SRC_DIR}/compression/factory.cc
    ${SRC_DIR}/compression/opener.cc
    ${SRC_DIR}/compression/stack_array.cc
    ${SRC_DIR}/compression/stream.cc
    ${SRC_DIR}/compression/zlib.cc
    ${SRC_DIR}/config/applier/endpoint.cc
    ${SRC_DIR}/config/applier/modules.cc
    ${SRC_DIR}/config/applier/state.cc
    ${SRC_DIR}/config/endpoint.cc
    ${SRC_DIR}/config/parser.cc
    ${SRC_DIR}/config/state.cc
    ${SRC_DIR}/file/cfile.cc
    ${SRC_DIR}/file/disk_accessor.cc
    ${SRC_DIR}/file/directory_event.cc
    ${SRC_DIR}/file/directory_watcher.cc
    ${SRC_DIR}/file/factory.cc
    ${SRC_DIR}/file/fifo.cc
    ${SRC_DIR}/file/opener.cc
    ${SRC_DIR}/file/splitter.cc
    ${SRC_DIR}/file/stream.cc
    ${SRC_DIR}/io/data.cc
    ${SRC_DIR}/io/endpoint.cc
    ${SRC_DIR}/io/events.cc
    ${SRC_DIR}/io/factory.cc
    ${SRC_DIR}/io/limit_endpoint.cc
    ${SRC_DIR}/io/protocols.cc
    ${SRC_DIR}/io/raw.cc
    ${SRC_DIR}/io/stream.cc
    ${SRC_DIR}/mapping/entry.cc
    ${SRC_DIR}/misc/diagnostic.cc
    ${SRC_DIR}/misc/filesystem.cc
    ${SRC_DIR}/misc/misc.cc
    ${SRC_DIR}/misc/processing_speed_computer.cc
    ${SRC_DIR}/misc/string.cc
    ${SRC_DIR}/misc/time.cc
    ${SRC_DIR}/misc/uuid.cc
    ${SRC_DIR}/misc/variant.cc
    ${SRC_DIR}/modules/handle.cc
    ${SRC_DIR}/persistent_cache.cc
    ${SRC_DIR}/persistent_file.cc
    ${SRC_DIR}/processing/acceptor.cc
    ${SRC_DIR}/processing/failover.cc
    ${SRC_DIR}/processing/feeder.cc
    ${SRC_DIR}/processing/stat_visitable.cc
    ${SRC_DIR}/stats/center.cc
    ${SRC_DIR}/stats/helper.cc
    ${SRC_DIR}/time/daterange.cc
    ${SRC_DIR}/time/timeperiod.cc
    ${SRC_DIR}/time/timerange.cc
    ${SRC_DIR}/time/timezone_locker.cc
    ${SRC_DIR}/time/timezone_manager.cc
  )

# Static libraries.
add_library(rokerbase STATIC ${LIBROKER_SOURCES})
set_target_properties(rokerbase PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(rokerbase REUSE_FROM multiplexing)
add_dependencies(rokerbase pb_broker_lib pb_bbdo_lib pb_extcmd_lib pb_neb_lib process_stat)
target_include_directories(rokerbase
                           PRIVATE ${PROJECT_SOURCE_DIR}/core)
target_link_libraries(
  rokerbase PRIVATE
  sql
  bbdo_bbdo
  pb_broker_lib
  bbdo
  ctncrypto
  pb_bbdo_lib
  pb_common_lib
  pb_extcmd_lib
  pb_open_telemetry_lib
  process_stat
  z
  spdlog::spdlog
  pthread
  dl)

add_library(roker STATIC ${SRC_DIR}/config/applier/init.cc)
target_link_libraries(roker PRIVATE rokerbase crypto ssl pthread dl)

# Standalone binary.
add_executable(cbd ${SRC_DIR}/main.cc)

add_dependencies(cbd multiplexing centreon_common pb_neb_lib)

set_target_properties(cbd PROPERTIES ENABLE_EXPORTS ON)

# Flags needed to include all symbols in binary.
target_link_libraries(
  cbd PRIVATE
  -Wl,--whole-archive
  log_v2
  sql
  rokerbase
  roker
  multiplexing
  centreon_common
  -Wl,--no-whole-archive
  nlohmann_json::nlohmann_json
  fmt::fmt
  -Wl,--whole-archive
  gRPC::grpc++
  protobuf::libprotobuf
  -Wl,--no-whole-archive
  stdc++fs)

# Centreon Broker Watchdog
option(WITH_CBWD "Build centreon broker watchdog." ON)

if(WITH_CBWD)
  add_subdirectory(watchdog)
endif()

# Module list.
unset(MODULE_LIST)

# sql library
add_subdirectory(core/sql)

if (ONLY_ROBOT_CMA)
  add_broker_module(STATS ON)
  add_broker_module(NEB ON)
  add_broker_module(RRD ON)
  add_broker_module(UNIFIED_SQL ON)
  add_broker_module(TCP ON)
else()
  # Generator module.
  add_broker_module(GENERATOR OFF)
  add_broker_module(STATS ON)
  add_broker_module(NEB ON)
  add_broker_module(RRD ON)
  add_broker_module(UNIFIED_SQL ON)
  add_broker_module(SQL ON)
  add_broker_module(STORAGE ON)
  add_broker_module(INFLUXDB ON)
  add_broker_module(GRAPHITE ON)
  add_broker_module(BAM ON)
  add_broker_module(TCP ON)
  add_broker_module(TLS ON)
  add_broker_module(TLS2 OFF)
  add_broker_module(DUMP OFF)
  add_broker_module(GRPC ON)
  add_broker_module(VICTORIA_METRICS ON)
  add_subdirectory(http_tsdb)
endif()

# Lua module.
option(WITH_MODULE_LUA "Build lua module." ON)

# Simu module.
option(WITH_MODULE_SIMU "Build simu module." OFF)

if(LUA_FOUND)
  if(WITH_MODULE_LUA)
    add_subdirectory("lua")
    list(APPEND MODULE_LIST "lua")
  endif()

  if(WITH_MODULE_SIMU)
    add_subdirectory("simu")
    list(APPEND MODULE_LIST "simu")
  endif()
endif()

# Format string.
string(REPLACE ";" ", " MODULE_LIST "${MODULE_LIST}")

#
# Install stuff.
#

# Create directories.
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

  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${PREFIX_BROKER_CONF}\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${PREFIX_MODULES}\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${PREFIX_CBMOD}\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${BROKER_VAR_LOG_DIR}\")
  mkdir_chown(\"${USER}\" \"${GROUP}\" \"${BROKER_VAR_LIB_DIR}\")
  if (WITH_STARTUP_SCRIPT STREQUAL \"sysv\")
    mkdir_chown(\"${USER}\" \"${GROUP}\" \"${STARTUP_DIR}\")
  endif ()
")

# Install rule.
install(TARGETS cbd RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_SBINDIR}")

# Install header files for development.
install(
  DIRECTORY "${INC_DIR}/../../../"
  DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}/centreon-broker"
  COMPONENT "development"
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.h")

#
# Packaging.
#
include(cmake/package.cmake)

#
# Unit tests.
#
# Enable testing.
option(WITH_SQL_TESTS "Sql unit tests are enabled." OFF)

if(WITH_TESTING AND NOT ONLY_ROBOT_CMA)
  set(TESTS_SOURCES
    ${TESTS_SOURCES}
    ${TEST_DIR}/bbdo/category.cc
    ${TEST_DIR}/bbdo/output.cc
    ${TEST_DIR}/bbdo/read.cc
    ${TEST_DIR}/cache/global_cache_test.cc
    ${TEST_DIR}/compression/stream/memory_stream.hh
    ${TEST_DIR}/compression/stream/read.cc
    ${TEST_DIR}/compression/stream/write.cc
    ${TEST_DIR}/compression/zlib/zlib.cc
    ${TEST_DIR}/config/init.cc
    ${TEST_DIR}/config/parser.cc
    ${TEST_DIR}/file/disk_accessor.cc
    ${TEST_DIR}/file/splitter/concurrent.cc
    ${TEST_DIR}/file/splitter/default.cc
    ${TEST_DIR}/file/splitter/more_than_max_size.cc
    ${TEST_DIR}/file/splitter/permission_denied.cc
    ${TEST_DIR}/file/splitter/resume.cc
    ${TEST_DIR}/file/splitter/split.cc
    ${TEST_DIR}/file/splitter/split_limited.cc
    ${TEST_DIR}/misc/buffer.cc
    ${TEST_DIR}/misc/exec.cc
    ${TEST_DIR}/misc/filesystem.cc
    ${TEST_DIR}/misc/math.cc
    ${TEST_DIR}/misc/misc.cc
    ${TEST_DIR}/misc/string.cc
    ${TEST_DIR}/modules/module.cc
    ${TEST_DIR}/processing/acceptor.cc
    ${TEST_DIR}/processing/feeder.cc
    ${TEST_DIR}/time/timerange.cc
    ${TEST_DIR}/rpc/brokerrpc.cc
    ${TEST_DIR}/exceptions.cc
    ${TEST_DIR}/io.cc
  )
  add_subdirectory(test)
endif()

#
# Print summary.
#
message(STATUS "")
message(STATUS "")
message(STATUS "Configuration Summary")
message(STATUS "---------------------")
message(STATUS "")
message(STATUS "  Project")
message(STATUS "    - Name                       Centreon Broker")
message(STATUS "    - Version                    ${CENTREON_BROKER_VERSION}")
message(STATUS "")
message(STATUS "  System")
message(STATUS "    - Name                       ${CMAKE_SYSTEM_NAME}")
message(STATUS "    - Version                    ${CMAKE_SYSTEM_VERSION}")
message(STATUS "    - Processor                  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  Build")
message(
  STATUS
    "    - Compiler                   ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})"
)
message(STATUS "    - Extra compilation flags    ${CMAKE_CXX_FLAGS}")

if(WITH_TESTING)
  message(STATUS "    - Unit tests                 enabled")

  if(MONITORING_ENGINE)
    message(STATUS "      - Monitoring engine        ${MONITORING_ENGINE}")
  else()
    message(STATUS "      - Monitoring engine        none")
  endif()

  if(WITH_COVERAGE)
    message(STATUS "      - Code coverage            enabled")
  else()
    message(STATUS "      - Code coverage            disabled")
  endif()
else()
  message(STATUS "    - Unit tests                 disabled")
endif()

if(WITH_STARTUP_SCRIPT STREQUAL "no")
  message(STATUS "    - Startup script             disabled")
else()
  message(STATUS "    - Startup script             ${WITH_STARTUP_SCRIPT}")
endif()

message(STATUS "    - Module                     ${MODULE_LIST}")
message(STATUS "")
message(STATUS "  Install")
message(STATUS "    - Prefix                     ${CMAKE_INSTALL_PREFIX}")

message(STATUS "    - Modules prefix             ${PREFIX_MODULES}")
message(
  STATUS
    "    - Include prefix             ${CMAKE_INSTALL_INCLUDEDIR}/centreon-broker"
)
message(STATUS "    - Configuration prefix       ${PREFIX_BROKER_CONF}")

if(NOT WITH_STARTUP_SCRIPT STREQUAL "no")
  message(STATUS "    - Startup dir                ${STARTUP_DIR}")
endif()

message(STATUS "    - User                       ${USER}")
message(STATUS "    - Group                      ${GROUP}")
message(STATUS "    - Package                    ${PACKAGE_LIST}")
message(STATUS "")
