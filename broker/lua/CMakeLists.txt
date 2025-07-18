#
# Copyright 2018-2021 Centreon
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

set(INC_DIR "${PROJECT_SOURCE_DIR}/lua/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/lua/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/lua/test")
include_directories(
  "${INC_DIR}" "${PROJECT_SOURCE_DIR}/neb/inc" "${PROJECT_SOURCE_DIR}/bam/inc"
  ${CMAKE_BINARY_DIR}/common/process_stat)
set(INC_DIR "${INC_DIR}/com/centreon/broker/lua")

# Storage module.
set(LUA "70-lua")
set(LUA
    "${LUA}"
    PARENT_SCOPE)
add_library(
  "${LUA}" SHARED
  # Sources.
  "${SRC_DIR}/broker_cache.cc"
  "${SRC_DIR}/broker_event.cc"
  "${SRC_DIR}/broker_log.cc"
  "${SRC_DIR}/broker_socket.cc"
  "${SRC_DIR}/broker_utils.cc"
  "${SRC_DIR}/connector.cc"
  "${SRC_DIR}/factory.cc"
  "${SRC_DIR}/luabinding.cc"
  "${SRC_DIR}/macro_cache.cc"
  "${SRC_DIR}/main.cc"
  "${SRC_DIR}/stream.cc"
  # Headers.
  "${INC_DIR}/broker_cache.hh"
  "${INC_DIR}/broker_event.hh"
  "${INC_DIR}/broker_log.hh"
  "${INC_DIR}/broker_socket.hh"
  "${INC_DIR}/broker_utils.hh"
  "${INC_DIR}/connector.hh"
  "${INC_DIR}/factory.hh"
  "${INC_DIR}/luabinding.hh"
  "${INC_DIR}/macro_cache.hh"
  "${INC_DIR}/stream.hh")
add_dependencies(${LUA} pb_neb_lib pb_storage_lib pb_bam_lib process_stat
                 pb_open_telemetry_lib)

target_link_libraries(
  "${LUA}"
  PRIVATE ${LUA_LIBRARIES}
          bbdo_bam
	  bbdo_storage
	  pb_storage_lib
          spdlog::spdlog)

target_precompile_headers(${LUA} PRIVATE precomp_inc/precomp.hpp)
set_target_properties(
  "${LUA}" PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY
                                "${CMAKE_BINARY_DIR}/broker/lib")

# Testing.
if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES} ${TEST_DIR}/lua.cc
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} "${LUA}"
      PARENT_SCOPE)
endif()

# Install rule.
install(TARGETS "${LUA}" LIBRARY DESTINATION "${PREFIX_MODULES}")
