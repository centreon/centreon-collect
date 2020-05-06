##
## Copyright 2009-2020 Centreon
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

find_package(gRPC CONFIG REQUIRED)
find_package(protoc CONFIG)
find_package(protobuf CONFIG)
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/broker/core/inc)

get_filename_component(hw_proto "${CMAKE_SOURCE_DIR}/broker/core/src/broker.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

set(hw_proto_srcs "${CMAKE_BINARY_DIR}/broker.pb.cc")
set(hw_proto_hdrs "${CMAKE_BINARY_DIR}/broker.pb.h")
set(hw_grpc_srcs "${CMAKE_BINARY_DIR}/broker.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_BINARY_DIR}/broker.grpc.pb.h")


add_custom_command(
  OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
  COMMAND protobuf::protoc
  ARGS --grpc_out "${CMAKE_BINARY_DIR}"
  --cpp_out "${CMAKE_BINARY_DIR}"
  -I "${hw_proto_path}"
  --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
  "${hw_proto}"
  DEPENDS "${hw_proto}")


add_library(
  berpc
  STATIC

  ${CMAKE_BINARY_DIR}/broker.grpc.pb.cc
  ${CMAKE_BINARY_DIR}/broker.pb.cc
  ${CMAKE_BINARY_DIR}/broker.grpc.pb.h
  ${CMAKE_BINARY_DIR}/broker.pb.h)
target_link_libraries(berpc CONAN_PKG::protobuf CONAN_PKG::grpc)
set_target_properties(berpc PROPERTIES COMPILE_FLAGS "-fPIC")

# Version.
add_definitions(-DCENTREON_BROKER_VERSION=\"${COLLECT_VERSION}\")
configure_file(
  ${CMAKE_SOURCE_DIR}/broker/core/inc/com/centreon/broker/version.hh.in
  ${CMAKE_BINARY_DIR}/broker-version.hh
  @ONLY
)
configure_file(
  ${CMAKE_SOURCE_DIR}/broker/core/inc/com/centreon/broker/vars.hh.in
  ${CMAKE_BINARY_DIR}/vars.hh
  @ONLY
)

include(FindPkgConfig)
# Find MySQL/Mariadb devel package
pkg_check_modules(_libmariadb libmariadb)
if (_libmariadb_FOUND)
  set(MYSQL_CFLAGS ${_libmariadb_CFLAGS})
  set(MYSQL_LIBS ${_libmariadb_LDFLAGS})
else ()
  pkg_check_modules(_mariadb mariadb)
  if (_mariadb_FOUND)
    set(MYSQL_CFLAGS ${_mariadb_CFLAGS})
    set(MYSQL_LIBS ${_mariadb_LDFLAGS})
  else ()
    pkg_check_modules(_mysql mysql)
    if (_mysql_FOUND)
      set(MYSQL_CFLAGS ${_mysql_CFLAGS})
      set(MYSQL_LIBS ${_mysql_LDFLAGS})
    else ()
      message(SEND_ERROR "MariaDB devel package is not installed")
    endif ()
  endif ()
endif ()

string(REPLACE "-" " -" MYSQL_CFLAGS ${MYSQL_CFLAGS})
message(STATUS ${MYSQL_CFLAGS})
message(STATUS ${MYSQL_LIBS})

# Broker vars
# Core library.
set(LIBROKER_SOURCES
  # Sources.
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/acceptor.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/ack.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/connector.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/factory.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/input.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/input_buffer.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/internal.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/output.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/stream.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/bbdo/version_response.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/broker_impl.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/brokerrpc.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/ceof/ceof_deserializer.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/ceof/ceof_iterator.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/ceof/ceof_parser.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/ceof/ceof_serializer.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/ceof/ceof_token.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/ceof/ceof_writer.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/compression/factory.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/compression/internal.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/compression/opener.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/compression/stack_array.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/compression/stream.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/compression/zlib.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/applier/endpoint.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/applier/modules.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/applier/state.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/endpoint.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/logger.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/parser.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/state.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/database_config.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/cfile.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/directory_event.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/directory_watcher.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/factory.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/fifo.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/internal.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/opener.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/splitter.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/file/stream.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/instance_broadcast.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/data.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/endpoint.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/event_info.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/events.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/factory.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/protocols.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/raw.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/io/stream.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/log_v2.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/logging/file.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/logging/logger.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/logging/logging.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/logging/manager.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/logging/syslogger.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/logging/temp_logger.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/mapping/entry.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/mapping/source.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/diagnostic.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/filesystem.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/global_lock.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/misc.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/processing_speed_computer.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/string.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/stringifier.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/tokenizer.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/uuid.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/misc/variant.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/modules/handle.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/modules/loader.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/multiplexing/engine.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/multiplexing/hooker.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/multiplexing/muxer.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/multiplexing/publisher.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/multiplexing/subscriber.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/mysql.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/database/mysql_bind.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/database/mysql_column.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/database/mysql_error.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/mysql_manager.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/database/mysql_result.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/database/mysql_stmt.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/mysql_connection.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/query_preparator.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/persistent_cache.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/persistent_file.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/processing/acceptor.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/processing/failover.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/processing/feeder.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/processing/stat_visitable.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/processing/thread.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/stats/helper.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/time/daterange.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/time/timeperiod.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/time/timerange.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/time/timezone_locker.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/time/timezone_manager.cc)

# Static libraries.
add_library(rokerbase STATIC ${LIBROKER_SOURCES})
set_target_properties(rokerbase PROPERTIES COMPILE_FLAGS "${MYSQL_CFLAGS} -fPIC")
target_link_libraries(rokerbase CONAN_PKG::json11 CONAN_PKG::spdlog CONAN_PKG::zlib ${MYSQL_LIBS}  pthread dl berpc)

add_library(roker STATIC
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/applier/init.cc
  ${CMAKE_SOURCE_DIR}/broker/core/src/config/applier/logger.cc)
target_link_libraries(roker rokerbase CONAN_PKG::grpc gRPC::grpc++_reflection)

# Standalone binary.
set(DAEMON cbd)
add_executable(${DAEMON} ${CMAKE_SOURCE_DIR}/broker/core/src/main.cc)

#Flags needed to include all symbols in binary.
target_link_libraries("${DAEMON}"
  "-Wl,--whole-archive" gRPC::grpc++_reflection rokerbase roker "-Wl,--no-whole-archive" CONAN_PKG::json11 CONAN_PKG::spdlog CONAN_PKG::fmt CONAN_PKG::grpc)

# Centreon Broker Watchdog
option(WITH_CBWD "Build centreon broker watchdong." ON)
if (WITH_CBWD)
  add_subdirectory(watchdog)
endif ()

#
# Unit tests.
#
# Enable testing.
option(WITH_TESTING "Generate unit tests." OFF)
if (WITH_TESTING)
  add_subdirectory(test)
endif ()
