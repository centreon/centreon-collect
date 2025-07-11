#
# Copyright 2021-2025 Centreon
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

add_library(pb_header_lib header.proto)
set_target_properties(pb_header_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_header_lib)

add_library(pb_neb_lib neb.proto)
add_dependencies(pb_neb_lib pb_header_lib)
set_target_properties(pb_neb_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_neb_lib)

add_library(pb_rebuild_message_lib rebuild_message.proto)
set_target_properties(pb_rebuild_message_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_rebuild_message_lib)

add_library(pb_remove_graph_message_lib remove_graph_message.proto)
set_target_properties(pb_remove_graph_message_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_remove_graph_message_lib)

add_library(pb_bbdo_lib bbdo.proto)
set_target_properties(pb_bbdo_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_bbdo_lib)

add_library(pb_storage_lib storage.proto)
set_target_properties(pb_storage_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_storage_lib)

add_library(pb_bam_lib bam.proto bam_state.proto)
add_dependencies(pb_bam_lib pb_header_lib)
set_target_properties(pb_bam_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_bam_lib)

add_library(pb_extcmd_lib extcmd.proto)
set_target_properties(pb_extcmd_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
protobuf_generate(TARGET pb_extcmd_lib)

set(otl_protobuf_files
    opentelemetry/proto/collector/metrics/v1/metrics_service
    opentelemetry/proto/metrics/v1/metrics
    opentelemetry/proto/common/v1/common
    opentelemetry/proto/resource/v1/resource
)

foreach(name IN LISTS otl_protobuf_files)
  set(proto_file "${name}.proto")
  add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
    COMMENT "Generating interface files of the otl file ${proto_file}"
    #DEPENDS ${CMAKE_BINARY_DIR}/opentelemetry-proto/${proto_file}
    DEPENDS opentelemetry-proto-files
    COMMAND
      protobuf::protoc ARGS --cpp_out=${CMAKE_SOURCE_DIR}/bbdo
      --proto_path=${CMAKE_SOURCE_DIR}/opentelemetry-proto ${proto_file}
    VERBATIM)
endforeach()


add_library(pb_open_telemetry_lib STATIC
${CMAKE_SOURCE_DIR}/bbdo/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc
${CMAKE_SOURCE_DIR}/bbdo/opentelemetry/proto/metrics/v1/metrics.pb.cc
${CMAKE_SOURCE_DIR}/bbdo/opentelemetry/proto/common/v1/common.pb.cc
${CMAKE_SOURCE_DIR}/bbdo/opentelemetry/proto/resource/v1/resource.pb.cc
)

target_include_directories(pb_open_telemetry_lib BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/bbdo)


set_target_properties(pb_open_telemetry_lib
                      PROPERTIES POSITION_INDEPENDENT_CODE ON)

macro(get_protobuf_files name)
  set_source_files_properties("${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
                              PROPERTIES GENERATED TRUE)
  set_source_files_properties("${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h"
                              PROPERTIES GENERATED TRUE)
  set(proto_${name} "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
                    "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h")
endmacro()

include_directories("${CMAKE_SOURCE_DIR}/broker/core/inc"
                    "${CMAKE_SOURCE_DIR}/broker/core/sql/inc")

add_library(
  bbdo_bbdo STATIC
  "bbdo/ack.cc"
  "bbdo/version_response.cc"
  "bbdo/stop.cc"
  "bbdo/ack.hh"
  "bbdo/version_response.hh"
  "bbdo/stop.hh"
  "bbdo/bbdo_version.hh")
set_target_properties(bbdo_bbdo PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(bbdo_bbdo PRIVATE precomp_inc/precomp.hpp)
add_dependencies(bbdo_bbdo pb_bbdo_lib pb_extcmd_lib)

add_library(
  bbdo_storage STATIC
  "storage/index_mapping.cc"
  "storage/metric_mapping.cc"
  "storage/metric.cc"
  "storage/remove_graph.cc"
  "storage/status.cc"
  "storage/metric.hh"
  "storage/remove_graph.hh"
  "storage/status.hh")
set_target_properties(bbdo_storage PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(bbdo_storage REUSE_FROM bbdo_bbdo)
add_dependencies(bbdo_storage table_max_size pb_storage_lib)
add_library(
  bbdo_bam STATIC
  "bam/ba_duration_event.cc"
  "bam/dimension_ba_bv_relation_event.hh"
  "bam/dimension_kpi_event.cc"
  "bam/dimension_timeperiod.hh"
  "bam/kpi_status.cc"
  "bam/ba_duration_event.hh"
  "bam/dimension_ba_event.cc"
  "bam/dimension_kpi_event.hh"
  "bam/dimension_truncate_table_signal.cc"
  "bam/kpi_status.hh"
  "bam/ba_event.cc"
  "bam/dimension_ba_event.hh"
  "bam/dimension_timeperiod.cc"
  "bam/dimension_truncate_table_signal.hh"
  "bam/rebuild.cc"
  "bam/ba_event.hh"
  "bam/dimension_ba_timeperiod_relation.cc"
  "bam/inherited_downtime.cc"
  "bam/rebuild.hh"
  "bam/ba_status.cc"
  "bam/dimension_ba_timeperiod_relation.hh"
  "bam/inherited_downtime.hh"
  "bam/ba_status.hh"
  "bam/dimension_bv_event.cc"
  "bam/kpi_event.cc"
  "bam/dimension_ba_bv_relation_event.cc"
  "bam/dimension_bv_event.hh"
  "bam/kpi_event.hh")
set_target_properties(bbdo_bam PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(bbdo_bam REUSE_FROM bbdo_bbdo)
add_dependencies(bbdo_bam table_max_size pb_bam_lib)
