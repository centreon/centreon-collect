#
# Copyright 2024-2025 Centreon
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
set(MODULE_DIR "${PROJECT_SOURCE_DIR}/modules/opentelemetry")
set(SRC_DIR "${MODULE_DIR}/src")

# protobuf service
set(service_files opentelemetry/proto/collector/metrics/v1/metrics_service)

foreach(name IN LISTS service_files)
  set(proto_file "${name}.proto")
  add_custom_command(
    OUTPUT "${SRC_DIR}/${name}.grpc.pb.cc"
    COMMENT "Generating grpc files of the otl service file ${proto_file}"
    DEPENDS opentelemetry-proto-files
    COMMAND
      ${Protobuf_PROTOC_EXECUTABLE} ARGS
      --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
      --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto --grpc_out=${SRC_DIR}
      ${proto_file}
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

endforeach()

# centagent server and client
add_custom_command(
  DEPENDS ${CMAKE_SOURCE_DIR}/agent/proto/agent.proto
  COMMENT
    "Generating interface files of the conf centreon_agent proto file (grpc)"
  OUTPUT ${SRC_DIR}/centreon_agent/agent.grpc.pb.cc
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    --proto_path=${CMAKE_SOURCE_DIR}/agent/proto
    --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    --grpc_out=${SRC_DIR}/centreon_agent
    ${CMAKE_SOURCE_DIR}/agent/proto/agent.proto
  DEPENDS ${CMAKE_SOURCE_DIR}/agent/proto/agent.proto
  COMMENT
    "Generating interface files of the conf centreon_agent proto file (protobuf)"
  OUTPUT ${SRC_DIR}/centreon_agent/agent.pb.cc
  COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${SRC_DIR}/centreon_agent
    --proto_path=${CMAKE_SOURCE_DIR}/agent/proto
    --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto
    ${CMAKE_SOURCE_DIR}/agent/proto/agent.proto
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# mod_externalcmd target.
add_library(
  opentelemetry SHARED
  ${SRC_DIR}/centreon_agent/agent.grpc.pb.cc
  ${SRC_DIR}/centreon_agent/agent.pb.cc
  ${SRC_DIR}/centreon_agent/agent_check_result_builder.cc
  ${SRC_DIR}/centreon_agent/agent_config.cc
  ${SRC_DIR}/centreon_agent/agent_impl.cc
  ${SRC_DIR}/centreon_agent/agent_reverse_client.cc
  ${SRC_DIR}/centreon_agent/agent_service.cc
  ${SRC_DIR}/centreon_agent/agent_stat.cc
  ${SRC_DIR}/centreon_agent/to_agent_connector.cc
  ${SRC_DIR}/grpc_config.cc
  ${SRC_DIR}/host_serv_extractor.cc
  ${SRC_DIR}/open_telemetry.cc
  ${SRC_DIR}/otl_config.cc
  ${SRC_DIR}/otl_check_result_builder.cc
  ${SRC_DIR}/otl_data_point.cc
  ${SRC_DIR}/otl_server.cc
  ${SRC_DIR}/main.cc
  ${SRC_DIR}/telegraf/conf_server.cc
  ${SRC_DIR}/telegraf/nagios_check_result_builder.cc
  ${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc
)

target_precompile_headers(opentelemetry PRIVATE precomp_inc/precomp.hh)

# set(EXTERNALCMD_MODULE "${EXTERNALCMD_MODULE}" PARENT_SCOPE)
target_link_libraries(
  opentelemetry
  PRIVATE spdlog::spdlog
          protobuf::libprotobuf
          Boost::program_options
          Boost::url
          centreon_http
          pb_open_telemetry_lib
          ctncrypto)

add_dependencies(opentelemetry pb_open_telemetry_lib pb_header_lib pb_neb_lib
                 engine_rpc)

target_include_directories(
  opentelemetry
  PRIVATE "${MODULE_DIR}/inc/com/centreon/engine/modules/opentelemetry"
          "${CMAKE_SOURCE_DIR}/bbdo"
          "${MODULE_DIR}/inc"
          ${CMAKE_SOURCE_DIR}/common/inc
          ${CMAKE_SOURCE_DIR}/common/http/inc
          ${CMAKE_SOURCE_DIR}/common/grpc/inc
          src
          ${PROJECT_SOURCE_DIR}/enginerpc
          ${CMAKE_SOURCE_DIR}/common/src)

# Install rule.
install(
  TARGETS "opentelemetry"
  DESTINATION "${ENGINE_MODULES_DIR}"
  COMPONENT "runtime")
