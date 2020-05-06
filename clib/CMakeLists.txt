##
## Copyright 2011-2014,2018-2019 Centreon
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

configure_file(
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/clib/version.hh.in
  ${CMAKE_BINARY_DIR}/clib-version.hh)

# Set headers.
set(
  HEADERS
  ${SPECIFIC_HEADERS}
)

if (WITH_TESTING)
  include(CTest)
  add_subdirectory(test)
endif ()


add_library(centreon-clib
  SHARED
  ${CMAKE_SOURCE_DIR}/clib/src/clib/version.cc
  ${CMAKE_SOURCE_DIR}/clib/src/exceptions/basic.cc
  ${CMAKE_SOURCE_DIR}/clib/src/exceptions/interruption.cc
  ${CMAKE_SOURCE_DIR}/clib/src/io/directory_entry.cc
  ${CMAKE_SOURCE_DIR}/clib/src/io/file_entry.cc
  ${CMAKE_SOURCE_DIR}/clib/src/io/file_stream.cc
  ${CMAKE_SOURCE_DIR}/clib/src/logging/syslogger.cc
  ${CMAKE_SOURCE_DIR}/clib/src/logging/backend.cc
  ${CMAKE_SOURCE_DIR}/clib/src/logging/engine.cc
  ${CMAKE_SOURCE_DIR}/clib/src/logging/file.cc
  ${CMAKE_SOURCE_DIR}/clib/src/logging/temp_logger.cc
  ${CMAKE_SOURCE_DIR}/clib/src/misc/argument.cc
  ${CMAKE_SOURCE_DIR}/clib/src/misc/command_line.cc
  ${CMAKE_SOURCE_DIR}/clib/src/misc/get_options.cc
  ${CMAKE_SOURCE_DIR}/clib/src/misc/stringifier.cc
  ${CMAKE_SOURCE_DIR}/clib/src/library.cc
  ${CMAKE_SOURCE_DIR}/clib/src/process_manager.cc
  ${CMAKE_SOURCE_DIR}/clib/src/process.cc
  ${CMAKE_SOURCE_DIR}/clib/src/handle_manager.cc
  ${CMAKE_SOURCE_DIR}/clib/src/handle_action.cc
  ${CMAKE_SOURCE_DIR}/clib/src/task_manager.cc
  ${CMAKE_SOURCE_DIR}/clib/src/timestamp.cc

  ${CMAKE_BINARY_DIR}/clib-version.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/exceptions/basic.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/exceptions/interruption.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/io/directory_entry.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/io/file_entry.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/io/file_stream.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/logging/syslogger.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/logging/backend.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/logging/engine.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/logging/file.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/logging/logger.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/logging/temp_logger.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/misc/argument.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/misc/command_line.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/misc/get_options.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/misc/stringifier.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/clib.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/handle.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/handle_action.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/handle_listener.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/handle_manager.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/hash.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/library.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/namespace.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/process_manager.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/process.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/task.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/task_manager.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/timestamp.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/unique_array_ptr.hh
  ${CMAKE_SOURCE_DIR}/clib/inc/com/centreon/unordered_hash.hh
  )

target_include_directories(centreon-clib PUBLIC ${CMAKE_SOURCE_DIR}/clib/inc)
target_include_directories(centreon-clib PUBLIC ${CMAKE_BINARY_DIR}/)
target_link_libraries(centreon-clib PUBLIC dl pthread)
target_compile_features(centreon-clib PUBLIC cxx_std_11)