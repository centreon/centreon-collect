##
## Copyright 2021 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##

macro(get_protobuf_files name)
    set(proto_file "${name}.proto")
    set(full_proto_file "${CMAKE_SOURCE_DIR}/bbdo/${name}.proto")
    add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc" "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h"
        DEPENDS ${full_proto_file}
        COMMENT "Generating interface files of the bbdo file ${proto_file}"
        COMMAND ${PROTOBUF_PREFIX}/bin/protoc
        ARGS --cpp_out=${CMAKE_SOURCE_DIR}/bbdo --proto_path=${CMAKE_SOURCE_DIR}/bbdo ${proto_file}
        VERBATIM
        )
  set_source_files_properties("${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc" PROPERTIES GENERATED TRUE)
  set_source_files_properties("${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h" PROPERTIES GENERATED TRUE)
  set(proto_${name}
      "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/${name}.pb.h")
endmacro()

macro(get_protobuf_accessor name)
    set(proto_file "${name}.proto")
    set(full_proto_file "${CMAKE_SOURCE_DIR}/bbdo/${name}.proto")
    add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/bbdo/${name}_accessor.hh"
        DEPENDS ${full_proto_file}
        COMMENT "Generating accessor to protobuf message ${proto_file}"
        COMMAND python3
        ARGS ${full_proto_file}
        VERBATIM
        )
endmacro()

macro(get_bbdo_files category)
  if (${category} STREQUAL "storage")
    set(bbdo_${category}
      "${CMAKE_SOURCE_DIR}/bbdo/storage/index_mapping.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/metric_mapping.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/metric.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/rebuild.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/remove_graph.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/status.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/metric.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/rebuild.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/remove_graph.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/storage/status.hh"
    )
  elseif (${category} STREQUAL "bam")
    set(bbdo_${category}
      "${CMAKE_SOURCE_DIR}/bbdo/bam/ba_duration_event.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_ba_bv_relation_event.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_kpi_event.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_timeperiod.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/kpi_status.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/ba_duration_event.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_ba_event.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_kpi_event.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_truncate_table_signal.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/kpi_status.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/ba_event.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_ba_event.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_timeperiod.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_truncate_table_signal.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/rebuild.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/ba_event.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_ba_timeperiod_relation.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_timeperiod_exception.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/inherited_downtime.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/rebuild.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/ba_status.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_ba_timeperiod_relation.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_timeperiod_exception.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/inherited_downtime.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/ba_status.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_bv_event.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_timeperiod_exclusion.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/kpi_event.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_ba_bv_relation_event.cc"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_bv_event.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/dimension_timeperiod_exclusion.hh"
      "${CMAKE_SOURCE_DIR}/bbdo/bam/kpi_event.hh"
  )
  endif ()
endmacro()
