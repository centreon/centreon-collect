#
# Copyright 2023-2024 Centreon
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
project("Centreon common" C CXX)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_subdirectory(log_v2)
  add_subdirectory(engine_conf)
endif()

# Set directories.
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc/com/centreon/common")
set(PROCESS_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/process/inc")
set(HTTP_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/http/inc/com/centreon/common/http")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/tests")

# Set sources.
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(SOURCES
      ${SRC_DIR}/hex_dump.cc ${SRC_DIR}/perfdata.cc ${SRC_DIR}/pool.cc
      ${SRC_DIR}/rapidjson_helper.cc ${SRC_DIR}/utf8.cc ${SRC_DIR}/file.cc)
else()
  add_compile_options("/wd5232")
  # we need not many things to just compile centreon-monitoring-agent
  # (centagent)
  set(SOURCES ${SRC_DIR}/perfdata.cc ${SRC_DIR}/rapidjson_helper.cc
              ${SRC_DIR}/utf8.cc)
endif()

# Include directories.
include_directories(${INCLUDE_DIR} ${HTTP_INCLUDE_DIR} ${VCPKG_INCLUDE_DIR}
                    ${PROCESS_INCLUDE_DIR})

add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
add_library(centreon_common STATIC ${SOURCES})
set_property(TARGET centreon_common PROPERTY POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(centreon_common PRIVATE precomp_inc/precomp.hh)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_subdirectory(http)
  add_subdirectory(vault)
  add_subdirectory(process_stat)
endif()

add_subdirectory(grpc)
add_subdirectory(process)
add_subdirectory(crypto)

if(WITH_TESTING)
  add_subdirectory(tests)
endif()
