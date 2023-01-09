#
# Copyright 2021-2022 Centreon
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

set(protobuf_files rebuild_message remove_graph_message neb severity tag
                   welcome)

foreach(name IN LISTS protobuf_files)
  set(proto_file "${name}.proto")
  set(full_proto_file "${CMAKE_SOURCE_DIR}/bbdo/${name}.proto")
  add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
           "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h"
    DEPENDS ${full_proto_file}
    COMMENT "Generating interface files of the bbdo file ${proto_file}"
    COMMAND
      ${Protobuf_PROTOC_EXECUTABLE} ARGS --cpp_out=${CMAKE_SOURCE_DIR}/bbdo
      --proto_path=${CMAKE_SOURCE_DIR}/bbdo ${proto_file}
    VERBATIM)

  add_custom_target(
    "target_${name}" DEPENDS "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
                             "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h")
endforeach()

add_library(pb_welcome_lib STATIC welcome.pb.cc welcome.pb.h)
add_dependencies(pb_welcome_lib target_welcome)
set_target_properties(pb_welcome_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(pb_neb_lib STATIC neb.pb.cc neb.pb.h)
add_dependencies(pb_neb_lib target_neb pb_tag_lib)
set_target_properties(pb_neb_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(pb_severity_lib STATIC severity.pb.cc severity.pb.h)
add_dependencies(pb_severity_lib target_neb)
set_target_properties(pb_severity_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(pb_tag_lib STATIC tag.pb.cc tag.pb.h)
add_dependencies(pb_tag_lib target_neb)
set_target_properties(pb_tag_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

macro(get_protobuf_files name)
  set_source_files_properties("${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
                              PROPERTIES GENERATED TRUE)
  set_source_files_properties("${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h"
                              PROPERTIES GENERATED TRUE)
  set(proto_${name} "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
                    "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h")
endmacro()

macro(get_protobuf_accessor name)
  set(proto_file "${name}.proto")
  set(full_proto_file "${CMAKE_SOURCE_DIR}/bbdo/${name}.proto")
  add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/bbdo/${name}_accessor.hh"
    DEPENDS ${full_proto_file}
    COMMENT "Generating accessor to protobuf message ${proto_file}"
    COMMAND python3 ARGS ${full_proto_file}
    VERBATIM)
endmacro()

include_directories("${CMAKE_SOURCE_DIR}/broker/core/inc")

add_library(
  bbdo_bbdo STATIC "bbdo/ack.cc" "bbdo/version_response.cc" "bbdo/stop.cc"
                   "bbdo/ack.hh" "bbdo/version_response.hh" "bbdo/stop.hh")
set_target_properties(bbdo_bbdo PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(bbdo_bbdo PRIVATE precomp_inc/precomp.hpp)

add_library(
  bbdo_storage STATIC
  "storage/index_mapping.cc"
  "storage/metric_mapping.cc"
  "storage/metric.cc"
  "storage/rebuild.cc"
  "storage/remove_graph.cc"
  "storage/status.cc"
  "storage/metric.hh"
  "storage/rebuild.hh"
  "storage/remove_graph.hh"
  "storage/status.hh")
set_target_properties(bbdo_storage PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(bbdo_storage REUSE_FROM bbdo_bbdo)
add_dependencies(bbdo_storage table_max_size)
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
  "bam/dimension_timeperiod_exception.cc"
  "bam/inherited_downtime.cc"
  "bam/rebuild.hh"
  "bam/ba_status.cc"
  "bam/dimension_ba_timeperiod_relation.hh"
  "bam/dimension_timeperiod_exception.hh"
  "bam/inherited_downtime.hh"
  "bam/ba_status.hh"
  "bam/dimension_bv_event.cc"
  "bam/dimension_timeperiod_exclusion.cc"
  "bam/kpi_event.cc"
  "bam/dimension_ba_bv_relation_event.cc"
  "bam/dimension_bv_event.hh"
  "bam/dimension_timeperiod_exclusion.hh"
  "bam/kpi_event.hh")
set_target_properties(bbdo_bam PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(bbdo_bam REUSE_FROM bbdo_bbdo)
add_dependencies(bbdo_bam table_max_size)
