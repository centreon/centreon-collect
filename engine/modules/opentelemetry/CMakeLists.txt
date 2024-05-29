#
# Copyright 2024 Centreon
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


#protobuf service
set(service_files
  opentelemetry/proto/collector/metrics/v1/metrics_service
)

foreach(name IN LISTS service_files)
  set(proto_file "${name}.proto")
  add_custom_command(
    OUTPUT "${SRC_DIR}/${name}.grpc.pb.cc"
    COMMENT "Generating grpc files of the otl service file ${proto_file}"
    DEPENDS opentelemetry-proto-files
    COMMAND
    ${Protobuf_PROTOC_EXECUTABLE} ARGS
    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
    --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto 
    --grpc_out=${SRC_DIR} ${proto_file}
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

endforeach()


# mod_externalcmd target.
add_library(opentelemetry SHARED 
${SRC_DIR}/data_point_fifo.cc
${SRC_DIR}/data_point_fifo_container.cc
${SRC_DIR}/grpc_config.cc
${SRC_DIR}/host_serv_extractor.cc
${SRC_DIR}/open_telemetry.cc
${SRC_DIR}/otl_config.cc
${SRC_DIR}/otl_check_result_builder.cc
${SRC_DIR}/otl_data_point.cc
${SRC_DIR}/otl_server.cc
${SRC_DIR}/main.cc
${SRC_DIR}/telegraf/conf_server.cc
${SRC_DIR}/telegraf/nagios_converter.cc
${SRC_DIR}/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc
)

target_precompile_headers(opentelemetry PRIVATE precomp_inc/precomp.hh)

# set(EXTERNALCMD_MODULE "${EXTERNALCMD_MODULE}" PARENT_SCOPE)
target_link_libraries(opentelemetry   spdlog::spdlog)

add_dependencies(opentelemetry 
  pb_open_telemetry_lib
  pb_neb_lib
  pb_tag_lib)

target_include_directories(opentelemetry PRIVATE 
  "${MODULE_DIR}/inc/com/centreon/engine/modules/opentelemetry" 
  "${CMAKE_SOURCE_DIR}/bbdo" 
  "${MODULE_DIR}/inc"
  ${CMAKE_SOURCE_DIR}/common/inc
  ${CMAKE_SOURCE_DIR}/common/http/inc 
  ${CMAKE_SOURCE_DIR}/common/grpc/inc 
  src
  ${PROJECT_SOURCE_DIR}/enginerpc 
  ${CMAKE_SOURCE_DIR}/common/src
  )

# Install rule.
install(
  TARGETS "opentelemetry"
  DESTINATION "${ENGINE_MODULES_DIR}"
  COMPONENT "runtime")
