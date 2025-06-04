#
# Copyright 2019-2025 Centreon (https://www.centreon.com/)
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

# Set directories.
set(INC_DIR "../inc/com/centreon/engine/")

# Include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/common/src
  ${CMAKE_BINARY_DIR}/common/process_stat
  ${CMAKE_SOURCE_DIR}/common/inc ${CMAKE_BINARY_DIR}/common/process_stat)

add_custom_command(
  DEPENDS engine.proto
  COMMENT "Generating interface files of the proto file (grpc)"
  OUTPUT engine.grpc.pb.cc engine.grpc.pb.h
  COMMAND
  protobuf::protoc ARGS --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    --proto_path=${CMAKE_SOURCE_DIR}/engine/enginerpc
    --proto_path=${CMAKE_SOURCE_DIR}/common/process_stat
    --grpc_out="${CMAKE_CURRENT_BINARY_DIR}" ${CMAKE_SOURCE_DIR}/engine/enginerpc/engine.proto
  DEPENDS engine.proto
  COMMENT "Generating interface files of the proto file (protobuf)"
  OUTPUT engine.pb.cc engine.pb.h
  COMMAND
    protobuf::protoc ARGS --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
    --proto_path=${CMAKE_SOURCE_DIR}/engine/enginerpc
    --proto_path=${CMAKE_SOURCE_DIR}/common/process_stat ${CMAKE_SOURCE_DIR}/engine/enginerpc/engine.proto
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/engine/enginerpc)

add_custom_target(
  engine_rpc
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/engine.grpc.pb.cc
          ${CMAKE_CURRENT_BINARY_DIR}/engine.pb.cc
          ${CMAKE_CURRENT_BINARY_DIR}/engine.grpc.pb.h
          ${CMAKE_CURRENT_BINARY_DIR}/engine.pb.h)

add_library(
  cerpc STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/engine.grpc.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/engine.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/engine.grpc.pb.h
  ${CMAKE_CURRENT_BINARY_DIR}/engine.pb.h)
target_link_libraries(cerpc PRIVATE protobuf::libprotobuf)
add_dependencies(cerpc engine_rpc process_stat)

# mod_enginerpc target.
add_library(
  enginerpc STATIC
  # Sources.
  engine_impl.cc enginerpc.cc
  # Headers.
  "${INC_DIR}/engine_impl.hh" "${INC_DIR}/enginerpc.hh")

add_dependencies(enginerpc centreon_common process_stat cerpc)
target_precompile_headers(enginerpc PRIVATE precomp_inc/precomp.hh)

# Prettier name.
set_property(
  TARGET enginerpc
  PROPERTY PREFIX ""
  PROPERTY POSITION_INDEPENDENT_CODE ON)
# Link target with libraries.
target_link_libraries(enginerpc PRIVATE cerpc centreon_common spdlog::spdlog)
