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

add_library(
  pb_configuration_lib STATIC
  state-generated.pb.cc
  state-generated.pb.h
  state-generated.cc
  state-generated.hh
  state.pb.cc
  state.pb.h
  anomalydetection_helper.cc
  contact_helper.hh
  message_helper.cc
  message_helper.hh
  severity_helper.cc
  anomalydetection_helper.hh
  hostdependency_helper.cc
  servicedependency_helper.cc
  severity_helper.hh
  command_helper.cc
  hostdependency_helper.hh
  servicedependency_helper.hh
  tag_helper.cc
  command_helper.hh
  hostescalation_helper.cc
  serviceescalation_helper.cc
  tag_helper.hh
  connector_helper.cc
  hostescalation_helper.hh
  serviceescalation_helper.hh
  timeperiod_helper.cc
  connector_helper.hh
  hostgroup_helper.cc
  servicegroup_helper.cc
  timeperiod_helper.hh
  contactgroup_helper.cc
  hostgroup_helper.hh
  servicegroup_helper.hh
  contactgroup_helper.hh
  host_helper.cc
  service_helper.cc
  contact_helper.cc
  host_helper.hh
  service_helper.hh)
add_dependencies(pb_configuration_lib target_configuration)
set_target_properties(pb_configuration_lib PROPERTIES POSITION_INDEPENDENT_CODE
                                                      ON)
