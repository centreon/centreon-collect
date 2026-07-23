#
# Copyright 2026 Centreon
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
add_definitions(${spdlog_DEFINITIONS})

# file_system.cc relies on Linux only syscalls (getdents64); the file_watcher is
# cross platform (inotify on Linux, ReadDirectoryChangesW on Windows).
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(FILE_SYSTEM_SOURCES file_watcher.cc)
else()
  set(FILE_SYSTEM_SOURCES file_watcher.cc)
endif()

add_library(
  centreon_file_system STATIC
  # Sources.
  ${FILE_SYSTEM_SOURCES})

set_target_properties(centreon_file_system
                      PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(centreon_file_system REUSE_FROM centreon_common)

