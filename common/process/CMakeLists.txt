#
# Copyright 2024 Centreon
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

include_directories(${PROJECT_SOURCE_DIR}/process/inc)
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
add_definitions(${spdlog_DEFINITIONS})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(SOURCES
    src/process_args.cc
    src/process.cc
    src/spawnp_launcher.cc)
else()
  set(SOURCES
    src/process_args.cc
    src/process.cc)
endif()


add_library(
  centreon_process STATIC
  ${SOURCES})

target_precompile_headers(centreon_process REUSE_FROM centreon_common)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  target_link_libraries(centreon_process INTERFACE Boost::process)
endif()

set_property(TARGET centreon_process PROPERTY POSITION_INDEPENDENT_CODE ON)
