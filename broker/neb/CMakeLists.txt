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
include_directories("${INC_DIR}" ${CMAKE_SOURCE_DIR}/common/inc)

# NEB sources.
set(NEB_SOURCES
    # Sources.
    # Headers.
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
    ${SRC_DIR}/status.cc
    ${INC_DIR}/com/centreon/broker/neb/acknowledgement.hh
    ${INC_DIR}/com/centreon/broker/neb/check.hh
    ${INC_DIR}/com/centreon/broker/neb/comment.hh
    ${INC_DIR}/com/centreon/broker/neb/custom_variable.hh
    ${INC_DIR}/com/centreon/broker/neb/custom_variable_status.hh
    ${INC_DIR}/com/centreon/broker/neb/downtime.hh
    ${INC_DIR}/com/centreon/broker/neb/events.hh
    ${INC_DIR}/com/centreon/broker/neb/group.hh
    ${INC_DIR}/com/centreon/broker/neb/group_member.hh
    ${INC_DIR}/com/centreon/broker/neb/host.hh
    ${INC_DIR}/com/centreon/broker/neb/host_check.hh
    ${INC_DIR}/com/centreon/broker/neb/host_group.hh
    ${INC_DIR}/com/centreon/broker/neb/host_group_member.hh
    ${INC_DIR}/com/centreon/broker/neb/host_parent.hh
    ${INC_DIR}/com/centreon/broker/neb/host_service.hh
    ${INC_DIR}/com/centreon/broker/neb/host_service_status.hh
    ${INC_DIR}/com/centreon/broker/neb/host_status.hh
    ${INC_DIR}/com/centreon/broker/neb/instance.hh
    ${INC_DIR}/com/centreon/broker/neb/instance_configuration.hh
    ${INC_DIR}/com/centreon/broker/neb/instance_status.hh
    ${INC_DIR}/com/centreon/broker/neb/log_entry.hh
    ${INC_DIR}/com/centreon/broker/neb/responsive_instance.hh
    ${INC_DIR}/com/centreon/broker/neb/service.hh
    ${INC_DIR}/com/centreon/broker/neb/service_check.hh
    ${INC_DIR}/com/centreon/broker/neb/service_group.hh
    ${INC_DIR}/com/centreon/broker/neb/service_group_member.hh
    ${INC_DIR}/com/centreon/broker/neb/service_status.hh
    ${INC_DIR}/com/centreon/broker/neb/status.hh)

# Static library.
add_library(nebbase STATIC ${NEB_SOURCES})
add_dependencies(
  nebbase
  table_max_size
  target_neb
  pb_neb_lib
  pb_common_lib
  pb_header_lib
  pb_open_telemetry_lib)

target_link_libraries(
  nebbase
  -L${PROTOBUF_LIB_DIR}
  protobuf
  pb_neb_lib
  pb_common_lib
  pb_header_lib
  pb_open_telemetry_lib)

target_include_directories(nebbase PRIVATE ${PROJECT_SOURCE_DIR}/bam/inc)

set(NEBBASE_CXXFLAGS "${NEBBASE_CXXFLAGS} -fPIC")
set_property(TARGET nebbase PROPERTY COMPILE_FLAGS ${NEBBASE_CXXFLAGS})
target_precompile_headers(nebbase PRIVATE precomp_inc/precomp.hpp)

# Centreon Broker module.
set(NEB "10-neb")
set(NEB
    "${NEB}"
    PARENT_SCOPE)
add_library(
  "${NEB}" SHARED
  # Main source.
  "${SRC_DIR}/broker.cc" # Inc
)

# Flags needed to include all symbols in binary.
target_link_libraries(${NEB} "-Wl,--whole-archive" nebbase
                      "-Wl,--no-whole-archive" spdlog::spdlog)

set_target_properties("${NEB}" PROPERTIES PREFIX "")
install(TARGETS "${NEB}" LIBRARY DESTINATION "${PREFIX_MODULES}")

# Centreon Engine/Nagios module.
add_library(cbmod SHARED ${SRC_DIR}/cbmod.cc
                         ${PROJECT_SOURCE_DIR}/core/src/config/applier/init.cc)

set_property(TARGET cbmod PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(cbmod PROPERTIES PREFIX "")
target_precompile_headers(cbmod PRIVATE precomp_inc/precomp.hpp)
target_link_libraries(
  cbmod
  fmt::fmt
  rokerbase
  multiplexing
  pb_neb_lib
  pb_header_lib
  protobuf
  nebbase
  log_v2
  centreon_common
  gRPC::grpc++)
get_property(
  CBMOD_DEFINES
  TARGET cbmod
  PROPERTY COMPILE_DEFINITIONS)
list(APPEND CBMOD_DEFINES CBMOD)
set_property(TARGET cbmod PROPERTY COMPILE_DEFINITIONS "${CBMOD_DEFINES}")

# target_include_directories(cbmod PRIVATE ${CMAKE_SOURCE_DIR}/bbdo)

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
