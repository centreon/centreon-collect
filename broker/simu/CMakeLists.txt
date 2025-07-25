#
# Copyright 2018 Centreon
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

set(INC_DIR "${PROJECT_SOURCE_DIR}/simu/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/simu/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/simu/test")
include_directories(
  "${INC_DIR}" "${PROJECT_SOURCE_DIR}/lua/inc" "${PROJECT_SOURCE_DIR}/neb/inc"
  ${CMAKE_BINARY_DIR}/common/process_stat
  "${PROJECT_SOURCE_DIR}/bam/inc")
set(INC_DIR "${INC_DIR}/com/centreon/broker/simu")

# Storage module.
set(SIMU "70-simu")
set(SIMU
    "${SIMU}"
    PARENT_SCOPE)
add_library(
  "${SIMU}" SHARED
  # Sources.
  "${SRC_DIR}/connector.cc"
  "${SRC_DIR}/factory.cc"
  "${SRC_DIR}/luabinding.cc"
  "${SRC_DIR}/main.cc"
  "${SRC_DIR}/stream.cc"
  # Headers.
  "${INC_DIR}/connector.hh"
  "${INC_DIR}/factory.hh"
  "${INC_DIR}/luabinding.hh"
  "${INC_DIR}/stream.hh")
add_dependencies(${SIMU} pb_neb_lib pb_header_lib pb_storage_lib pb_bam_lib
                 process_stat
                 pb_open_telemetry_lib)

target_link_libraries("${SIMU}" PRIVATE ${LUA_LIBRARIES} spdlog::spdlog)
target_precompile_headers(${SIMU} PRIVATE precomp_inc/precomp.hpp)
set_target_properties(
  "${SIMU}" PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY
                                 "${CMAKE_BINARY_DIR}/broker/lib")

# Testing.
if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES} "${TEST_DIR}/simu.cc"
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} "${SIMU}"
      PARENT_SCOPE)
endif()

# Install rule.
install(TARGETS "${SIMU}" LIBRARY DESTINATION "${PREFIX_MODULES}")
