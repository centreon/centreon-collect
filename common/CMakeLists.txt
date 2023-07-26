#
# Copyright 2023 Centreon
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
project("Centreon common" C CXX)

add_subdirectory(configuration)

# Set directories.
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")

add_custom_command(
  DEPENDS ${SRC_DIR}/process_stat.proto
  COMMENT "Generating interface files of the proto file (grpc)"
  OUTPUT ${SRC_DIR}/process_stat.grpc.pb.cc ${SRC_DIR}/process_stat.grpc.pb.h
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${CONAN_BIN_DIRS_GRPC}/grpc_cpp_plugin
    --proto_path=${SRC_DIR} --grpc_out="${SRC_DIR}"
    ${SRC_DIR}/process_stat.proto
  DEPENDS ${SRC_DIR}/process_stat.proto
  COMMENT "Generating interface files of the proto file (protobuf)"
  OUTPUT ${SRC_DIR}/process_stat.pb.cc ${SRC_DIR}/process_stat.pb.h
  COMMAND ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out="${SRC_DIR}"
          --proto_path=${SRC_DIR} ${SRC_DIR}/process_stat.proto)

# Set sources.
set(SOURCES ${SRC_DIR}/process_stat.cc ${SRC_DIR}/process_stat.pb.cc
            ${SRC_DIR}/process_stat.grpc.pb.cc ${SRC_DIR}/hex_dump.cc)

# Include directories.
include_directories("${INCLUDE_DIR}")

add_library(centreon_common STATIC ${SOURCES})
set_property(TARGET centreon_common PROPERTY POSITION_INDEPENDENT_CODE ON)

if(WITH_TESTING)
  add_subdirectory(test)
endif()
