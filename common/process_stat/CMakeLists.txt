#
# Copyright 2025 Centreon
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
  DEPENDS process_stat.proto
  COMMENT "Generating interface files of the proto file (grpc)"
  OUTPUT process_stat.grpc.pb.cc process_stat.grpc.pb.h
  COMMAND
    protobuf::protoc ARGS
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
    --grpc_out="${CMAKE_BINARY_DIR}/common/process_stat" process_stat.proto
  DEPENDS process_stat.proto
  COMMENT "Generating interface files of the proto file (protobuf)"
  OUTPUT process_stat.pb.cc process_stat.pb.h
  COMMAND protobuf::protoc ARGS --cpp_out="${CMAKE_BINARY_DIR}/common/process_stat"
  --proto_path=${CMAKE_CURRENT_SOURCE_DIR} process_stat.proto
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_library(process_stat STATIC
  ${CMAKE_BINARY_DIR}/common/process_stat/process_stat.grpc.pb.cc
  ${CMAKE_BINARY_DIR}/common/process_stat/process_stat.pb.cc
  process_stat.cc
)

set_target_properties(process_stat PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

target_precompile_headers(process_stat REUSE_FROM centreon_common)
