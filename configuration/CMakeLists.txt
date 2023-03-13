#
# Copyright 2022 Centreon
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

add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/configuration/state.pb.cc
         ${CMAKE_SOURCE_DIR}/configuration/state.pb.h
  DEPENDS ${CMAKE_SOURCE_DIR}/configuration/state.proto
  COMMENT "Generating interface files of the engine configuration file"
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --cpp_out=${CMAKE_SOURCE_DIR}/configuration
    --proto_path=${CMAKE_SOURCE_DIR}/configuration
    ${CMAKE_SOURCE_DIR}/configuration/state.proto
  VERBATIM)

add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/configuration/state-generated.pb.cc
         ${CMAKE_SOURCE_DIR}/configuration/state-generated.pb.h
  DEPENDS ${CMAKE_SOURCE_DIR}/configuration/state-generated.proto
  COMMENT "Generating interface files of the engine configuration file"
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --cpp_out=${CMAKE_SOURCE_DIR}/configuration
    --proto_path=${CMAKE_SOURCE_DIR}/configuration
    ${CMAKE_SOURCE_DIR}/configuration/state-generated.proto
  VERBATIM)

add_custom_target(
  target_configuration DEPENDS state.pb.cc state.pb.h state-generated.pb.cc
                               state-generated.pb.h)

include_directories("${CMAKE_SOURCE_DIR}/engine/inc")

add_definitions(-DDEFAULT_STATUS_FILE="${ENGINE_VAR_LOG_DIR}/status.dat")
add_definitions(-DDEFAULT_RETENTION_FILE="${ENGINE_VAR_LOG_DIR}/retention.dat")
add_definitions(-DDEFAULT_DEBUG_FILE="${ENGINE_VAR_LOG_DIR}/centengine.debug")
add_definitions(
  -DDEFAULT_COMMAND_FILE="${ENGINE_VAR_LIB_DIR}/rw/centengine.cmd")
add_definitions(-DDEFAULT_LOG_FILE="${ENGINE_VAR_LOG_DIR}/centengine.log")

add_library(
  pb_configuration_lib STATIC
  anomalydetection_helper.cc
  anomalydetection_helper.hh
  command_helper.cc
  command_helper.hh
  connector_helper.cc
  connector_helper.hh
  contact_helper.cc
  contact_helper.hh
  contactgroup_helper.cc
  contactgroup_helper.hh
  host_helper.cc
  host_helper.hh
  hostdependency_helper.cc
  hostdependency_helper.hh
  hostescalation_helper.cc
  hostescalation_helper.hh
  hostgroup_helper.cc
  hostgroup_helper.hh
  message_helper.cc
  message_helper.hh
  service_helper.cc
  service_helper.hh
  servicedependency_helper.cc
  servicedependency_helper.hh
  serviceescalation_helper.cc
  serviceescalation_helper.hh
  servicegroup_helper.cc
  servicegroup_helper.hh
  severity_helper.cc
  severity_helper.hh
  state-generated.pb.cc
  state-generated.pb.h
  state.pb.cc
  state.pb.h
  state_helper.cc
  state_helper.hh
  tag_helper.cc
  tag_helper.hh
  timeperiod_helper.cc
  timeperiod_helper.hh
  )
add_dependencies(pb_configuration_lib target_configuration)
set_target_properties(pb_configuration_lib PROPERTIES POSITION_INDEPENDENT_CODE
                                                      ON)
