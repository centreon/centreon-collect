#
# Copyright 2011 Centreon
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
set(MODULE_DIR "${PROJECT_SOURCE_DIR}/grpc")
set(INC_DIR "${MODULE_DIR}/inc/com/centreon/broker/grpc")
set(SRC_DIR "${MODULE_DIR}/src")
set(TEST_DIR "${MODULE_DIR}/test")
include_directories(${MODULE_DIR}/inc 
  ${SRC_DIR} 
  ${CMAKE_SOURCE_DIR}/bbdo
  ${CMAKE_SOURCE_DIR}/common 
  ${CMAKE_SOURCE_DIR}/common/grpc/inc)

# Sources.
set(SOURCES
    "${SRC_DIR}/acceptor.cc"
    "${SRC_DIR}/connector.cc"
    "${SRC_DIR}/factory.cc"
    "${SRC_DIR}/main.cc"
    "${SRC_DIR}/stream.cc"
    "${SRC_DIR}/grpc_stream.grpc.pb.cc"
    "${SRC_DIR}/grpc_stream.pb.cc"
    "${SRC_DIR}/grpc_bridge.cc"
    "${PROJECT_SOURCE_DIR}/core/src/io/limit_endpoint.cc"
)

# GRPC module.
set(GRPC "50-grpc")
set(GRPC
    ${GRPC}
    PARENT_SCOPE)
add_library(${GRPC} SHARED ${SOURCES})
set_target_properties(${GRPC} PROPERTIES PREFIX ""
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")
target_link_libraries(
  ${GRPC}
  centreon_grpc
  #"-Wl,--whole-archive"
  pb_neb_lib
  pb_common_lib
  pb_storage_lib
  pb_bbdo_lib
  pb_bam_lib
  pb_extcmd_lib
  pb_open_telemetry_lib
  pb_rebuild_message_lib
  pb_remove_graph_message_lib
  pb_header_lib
  centreon_common
  #"-Wl,--no-whole-archive"
  spdlog::spdlog)

target_precompile_headers(${GRPC} PRIVATE precomp_inc/precomp.hh)

file(
  GLOB bbdo_files
  LIST_DIRECTORIES false
  ${CMAKE_SOURCE_DIR}/bbdo/*.proto)

add_custom_command(
  OUTPUT ${MODULE_DIR}/grpc_stream.proto ${MODULE_DIR}/src/grpc_bridge.cc
  COMMENT "Generating grpc_stream.proto from bbdo protobuf files"
  DEPENDS ${bbdo_files}
  COMMAND
    python3 ARGS ${MODULE_DIR}/generate_proto.py -f
    ${MODULE_DIR}/grpc_stream.proto -c ${MODULE_DIR}/src/grpc_bridge.cc -d
    ${CMAKE_SOURCE_DIR}/bbdo
  VERBATIM)

add_custom_command(
  OUTPUT "${SRC_DIR}/grpc_stream.pb.cc" "${SRC_DIR}/grpc_stream.pb.h"
  DEPENDS ${MODULE_DIR}/grpc_stream.proto
  COMMENT "Generating interface files of grpc_stream.proto"
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${SRC_DIR}
    --proto_path=${MODULE_DIR} --proto_path=${CMAKE_SOURCE_DIR}/bbdo
    --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    grpc_stream.proto
  VERBATIM)

add_custom_command(
  OUTPUT "${SRC_DIR}/grpc_stream.grpc.pb.cc" "${SRC_DIR}/grpc_stream.grpc.pb.h"
  DEPENDS ${MODULE_DIR}/grpc_stream.proto
  COMMENT "Generating interface files grpc.pb of grpc_stream.proto"
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS --grpc_out=${SRC_DIR}
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    --proto_path=${MODULE_DIR} --proto_path=${CMAKE_SOURCE_DIR}/bbdo
    --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    grpc_stream.proto
  VERBATIM
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Testing.
if(WITH_TESTING)
  set(TESTS_SOURCES
      ${TESTS_SOURCES} ${TEST_DIR}/acceptor.cc
      ${TEST_DIR}/stream_test.cc ${TEST_DIR}/factory_test.cc
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} ${GRPC}
      PARENT_SCOPE)
endif(WITH_TESTING)

# Install rule.
install(TARGETS "${GRPC}" LIBRARY DESTINATION "${PREFIX_MODULES}")
