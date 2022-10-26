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

add_custom_target(target_configuration DEPENDS state.pb.cc state.pb.h)

add_library(pb_configuration_lib STATIC state.pb.cc state.pb.h)
add_dependencies(pb_configuration_lib target_configuration)
set_target_properties(pb_configuration_lib PROPERTIES POSITION_INDEPENDENT_CODE
                                                      ON)
