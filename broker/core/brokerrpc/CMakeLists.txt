#
# Copyright 2009-2026 Centreon
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

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

# gRPC filkes generation
add_custom_command(
  OUTPUT
    ${PROJECT_BINARY_DIR}/broker.pb.cc
    ${PROJECT_BINARY_DIR}/broker.pb.h
    ${PROJECT_BINARY_DIR}/broker.grpc.pb.cc
    ${PROJECT_BINARY_DIR}/broker.grpc.pb.h
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE}
    --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
    --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
    --proto_path=${CMAKE_SOURCE_DIR}/bbdo
    --proto_path=${CMAKE_SOURCE_DIR}/common/engine_conf
    --proto_path=${CMAKE_SOURCE_DIR}/common/process_stat
    --grpc_out=${PROJECT_BINARY_DIR}
    --cpp_out=${PROJECT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.proto
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.proto
  COMMENT
    "Generating gRPC and Protobuf files for broker.proto"
  VERBATIM
)

set_source_files_properties(
  ${PROJECT_BINARY_DIR}/broker.pb.cc
  ${PROJECT_BINARY_DIR}/broker.pb.h
  ${PROJECT_BINARY_DIR}/broker.grpc.pb.cc
  ${PROJECT_BINARY_DIR}/broker.grpc.pb.h
  PROPERTIES GENERATED TRUE
)

# Dependencies Target
add_custom_target(target_broker_message
  DEPENDS
    ${PROJECT_BINARY_DIR}/broker.pb.cc
    ${PROJECT_BINARY_DIR}/broker.pb.h
    ${PROJECT_BINARY_DIR}/broker.grpc.pb.cc
    ${PROJECT_BINARY_DIR}/broker.grpc.pb.h
)

# Library with generated files
add_library(pb_broker_lib STATIC
  ${PROJECT_BINARY_DIR}/broker.pb.cc
  ${PROJECT_BINARY_DIR}/broker.grpc.pb.cc
)

add_dependencies(pb_broker_lib target_broker_message process_stat pb_common_lib engine_conf)

target_include_directories(pb_broker_lib
  PUBLIC
    ${PROJECT_BINARY_DIR}  # Needed for generated files
)

target_link_libraries(pb_broker_lib
  PRIVATE
    pb_common_lib
    pb_neb_lib
    process_stat
    engine_conf
)

set_target_properties(pb_broker_lib PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Bibliothèque brokerrpc
add_library(brokerrpc
  broker_impl.cc
  brokerrpc.cc
)

target_precompile_headers(brokerrpc PRIVATE ../precomp_inc/precomp.hpp)
add_dependencies(brokerrpc pb_broker_lib)

target_link_libraries(brokerrpc
  PRIVATE
    pb_broker_lib
)

set_target_properties(brokerrpc PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)
