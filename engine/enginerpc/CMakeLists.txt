#
# Copyright 2019-2021 Centreon (https://www.centreon.com/)
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
set(ENGINERPC_DIR "${PROJECT_SOURCE_DIR}/enginerpc")
set(ENGINERPC_DIR
    "${PROJECT_SOURCE_DIR}/enginerpc"
    PARENT_SCOPE)
set(INC_DIR "../inc/com/centreon/engine/")

# Include directories.
include_directories(${ENGINERPC_DIR} ${CMAKE_SOURCE_DIR}/common/src
                    ${CMAKE_SOURCE_DIR}/common/inc ${CONAN_INCLUDE_DIRS_GRPC})

add_custom_command(
  DEPENDS ${ENGINERPC_DIR}/engine.proto
  COMMENT "Generating interface files of the proto file (grpc)"
  OUTPUT ${ENGINERPC_DIR}/engine.grpc.pb.cc ${ENGINERPC_DIR}/engine.grpc.pb.h
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${CONAN_BIN_DIRS_GRPC}/grpc_cpp_plugin
    --proto_path=${ENGINERPC_DIR} --proto_path=${CMAKE_SOURCE_DIR}/common/src
    --grpc_out="${ENGINERPC_DIR}" ${ENGINERPC_DIR}/engine.proto
  DEPENDS ${ENGINERPC_DIR}/engine.proto
  COMMENT "Generating interface files of the proto file (protobuf)"
  OUTPUT ${ENGINERPC_DIR}/engine.pb.cc ${ENGINERPC_DIR}/engine.pb.h
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out="${ENGINERPC_DIR}"
    --proto_path=${ENGINERPC_DIR} --proto_path=${CMAKE_SOURCE_DIR}/common/src
    ${ENGINERPC_DIR}/engine.proto)

add_custom_target(
  engine_rpc
  DEPENDS ${ENGINERPC_DIR}/engine.grpc.pb.cc ${ENGINERPC_DIR}/engine.pb.cc
          ${ENGINERPC_DIR}/engine.grpc.pb.h ${ENGINERPC_DIR}/engine.pb.h)

add_library(
  cerpc STATIC ${ENGINERPC_DIR}/engine.grpc.pb.cc ${ENGINERPC_DIR}/engine.pb.cc
               ${ENGINERPC_DIR}/engine.grpc.pb.h ${ENGINERPC_DIR}/engine.pb.h)
target_link_libraries(cerpc CONAN_PKG::protobuf)

# mod_enginerpc target.
add_library(
  enginerpc_legacy STATIC
  # Sources.
  engine_impl.cc enginerpc.cc
  # Headers.
  "${INC_DIR}/engine_impl.hh" "${INC_DIR}/enginerpc.hh")

add_dependencies(enginerpc_legacy centreon_common)

target_precompile_headers(enginerpc_legacy PRIVATE precomp_inc/precomp.hh)

# Prettier name.
set_property(
  TARGET enginerpc_legacy
  PROPERTY PREFIX ""
  PROPERTY POSITION_INDEPENDENT_CODE ON)
# Link target with libraries.
target_link_libraries(enginerpc_legacy cerpc centreon_common CONAN_PKG::spdlog)

add_library(
  enginerpc STATIC
  # Sources.
  engine_impl.cc enginerpc.cc
  # Headers.
  "${INC_DIR}/engine_impl.hh" "${INC_DIR}/enginerpc.hh")

add_dependencies(enginerpc centreon_common)

target_precompile_headers(enginerpc PRIVATE precomp_inc/precomp.hh)

# Prettier name.
set_property(
  TARGET enginerpc
  PROPERTY PREFIX ""
  PROPERTY POSITION_INDEPENDENT_CODE ON)
# Link target with libraries.
target_link_libraries(enginerpc cerpc centreon_common CONAN_PKG::spdlog)
