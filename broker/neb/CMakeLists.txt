#
# Copyright 2009-2013,2015-2025 Centreon
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
set(INC_DIR "${PROJECT_SOURCE_DIR}/neb/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/neb/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/neb/test")
include_directories(
  "${INC_DIR}" ${CMAKE_SOURCE_DIR}/common/inc ${CMAKE_BINARY_DIR}/bbdo
  ${CMAKE_BINARY_DIR}/common/process_stat)

# NEB sources.
set(NEB_SOURCES
    ${SRC_DIR}/acknowledgement.cc
    ${SRC_DIR}/bbdo2_to_bbdo3.cc
    ${SRC_DIR}/check.cc
    ${SRC_DIR}/comment.cc
    ${SRC_DIR}/custom_variable.cc
    ${SRC_DIR}/custom_variable_status.cc
    ${SRC_DIR}/downtime.cc
    ${SRC_DIR}/group.cc
    ${SRC_DIR}/group_member.cc
    ${SRC_DIR}/host.cc
    ${SRC_DIR}/host_check.cc
    ${SRC_DIR}/host_group.cc
    ${SRC_DIR}/host_group_member.cc
    ${SRC_DIR}/host_parent.cc
    ${SRC_DIR}/host_service.cc
    ${SRC_DIR}/host_service_status.cc
    ${SRC_DIR}/host_status.cc
    ${SRC_DIR}/instance.cc
    ${SRC_DIR}/instance_configuration.cc
    ${SRC_DIR}/instance_status.cc
    ${SRC_DIR}/log_entry.cc
    ${SRC_DIR}/responsive_instance.cc
    ${SRC_DIR}/service.cc
    ${SRC_DIR}/service_check.cc
    ${SRC_DIR}/service_group.cc
    ${SRC_DIR}/service_group_member.cc
    ${SRC_DIR}/service_status.cc
    ${SRC_DIR}/status.cc)

# Static library.
add_library(nebbase STATIC ${NEB_SOURCES})
add_dependencies(nebbase process_stat pb_common_lib table_max_size pb_neb_lib
                 pb_open_telemetry_lib)

target_include_directories(nebbase PRIVATE ${PROJECT_SOURCE_DIR}/bam/inc)
target_link_libraries(nebbase PRIVATE pb_neb_lib pb_header_lib pb_common_lib
                                      pb_open_telemetry_lib)

set_target_properties(nebbase PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(nebbase PRIVATE precomp_inc/precomp.hpp)

# Centreon Broker module.
set(NEB "10-neb")
set(NEB
    "${NEB}"
    PARENT_SCOPE)
add_library(
  "${NEB}" SHARED
  # Main source.
  ${SRC_DIR}/broker.cc)

add_dependencies(${NEB} nebbase)

# Flags needed to include all symbols in binary.
target_link_libraries(${NEB} PRIVATE -Wl,--whole-archive nebbase -Wl,--no-whole-archive spdlog::spdlog)

set_target_properties(
  ${NEB} PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY
                              ${CMAKE_BINARY_DIR}/broker/lib)

target_precompile_headers(${NEB} REUSE_FROM nebbase)
install(TARGETS "${NEB}" LIBRARY DESTINATION "${PREFIX_MODULES}")

# Centreon Engine/Nagios module.
add_library(cbmod SHARED ${SRC_DIR}/cbmod.cc
                         ${PROJECT_SOURCE_DIR}/core/src/config/applier/init.cc)

set_target_properties(cbmod PROPERTIES PREFIX "" POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(cbmod REUSE_FROM nebbase)
target_link_libraries(
  cbmod
  PRIVATE fmt::fmt
          -Wl,--whole-archive
          rokerbase
          process_stat
          centreon_common
          nebbase
	  gRPC::grpc++
          -Wl,--no-whole-archive
	  pb_bam_lib
	  pb_storage_lib
          multiplexing
          pb_common_lib
          pb_neb_lib
          pb_header_lib
          log_v2)

get_property(
  CBMOD_DEFINES
  TARGET cbmod
  PROPERTY COMPILE_DEFINITIONS)
list(APPEND CBMOD_DEFINES CBMOD)
set_property(TARGET cbmod PROPERTY COMPILE_DEFINITIONS "${CBMOD_DEFINES}")

# Testing.
if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES}
      # Actual tests
      ${TEST_DIR}/bbdo2_to_bbdo3_test.cc
      ${TEST_DIR}/custom_variable.cc
      ${TEST_DIR}/custom_variable_status.cc
      ${TEST_DIR}/host.cc
      ${TEST_DIR}/host_check.cc
      ${TEST_DIR}/host_parent.cc
      ${TEST_DIR}/host_status.cc
      ${TEST_DIR}/instance.cc
      ${TEST_DIR}/instance_status.cc
      ${TEST_DIR}/log_entry.cc
      ${TEST_DIR}/randomize.cc
      ${TEST_DIR}/randomize.hh
      ${TEST_DIR}/service.cc
      ${TEST_DIR}/service_check.cc
      ${TEST_DIR}/service_status.cc
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} ${NEB}
      PARENT_SCOPE)
endif()

# Install rules.
install(TARGETS cbmod LIBRARY DESTINATION "${PREFIX_CBMOD}")
