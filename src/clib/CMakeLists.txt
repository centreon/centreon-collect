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
  ${CMAKE_SOURCE_DIR}/include/com/centreon/clib/version.hh.in
  ${CMAKE_BINARY_DIR}/clib-version.hh)

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include/)

add_library(centreon_clib
  SHARED
  ${CMAKE_SOURCE_DIR}/src/clib/clib/version.cc
  ${CMAKE_SOURCE_DIR}/src/clib/exceptions/basic.cc
  ${CMAKE_SOURCE_DIR}/src/clib/exceptions/interruption.cc
  ${CMAKE_SOURCE_DIR}/src/clib/io/directory_entry.cc
  ${CMAKE_SOURCE_DIR}/src/clib/io/file_entry.cc
  ${CMAKE_SOURCE_DIR}/src/clib/io/file_stream.cc
  ${CMAKE_SOURCE_DIR}/src/clib/logging/syslogger.cc
  ${CMAKE_SOURCE_DIR}/src/clib/logging/backend.cc
  ${CMAKE_SOURCE_DIR}/src/clib/logging/engine.cc
  ${CMAKE_SOURCE_DIR}/src/clib/logging/file.cc
  ${CMAKE_SOURCE_DIR}/src/clib/logging/temp_logger.cc
  ${CMAKE_SOURCE_DIR}/src/clib/misc/argument.cc
  ${CMAKE_SOURCE_DIR}/src/clib/misc/command_line.cc
  ${CMAKE_SOURCE_DIR}/src/clib/misc/get_options.cc
  ${CMAKE_SOURCE_DIR}/src/clib/misc/stringifier.cc
  ${CMAKE_SOURCE_DIR}/src/clib/library.cc
  ${CMAKE_SOURCE_DIR}/src/clib/process_manager.cc
  ${CMAKE_SOURCE_DIR}/src/clib/process.cc
  ${CMAKE_SOURCE_DIR}/src/clib/handle_manager.cc
  ${CMAKE_SOURCE_DIR}/src/clib/handle_action.cc
  ${CMAKE_SOURCE_DIR}/src/clib/task_manager.cc
  ${CMAKE_SOURCE_DIR}/src/clib/timestamp.cc
  )

target_include_directories(centreon_clib PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(centreon_clib PUBLIC ${CMAKE_SOURCE_DIR}/include/)
target_link_libraries(centreon_clib PUBLIC dl pthread)
target_compile_features(centreon_clib PUBLIC cxx_std_11)

install(TARGETS centreon_clib COMPONENT clib DESTINATION ${CMAKE_INSTALL_LIBDIR})
