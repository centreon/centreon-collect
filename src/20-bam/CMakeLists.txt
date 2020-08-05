##
## Copyright 2011-2016 Centreon
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

# Global options.
include(${CMAKE_SOURCE_DIR}/cmake/FindMariaDB.cmake)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CONAN_INCLUDE_DIRS_JSON11})
include_directories(${CONAN_INCLUDE_DIRS_SPDLOG})
# BAM module.
add_library(20-bam SHARED
  # Sources.
  ${CMAKE_SOURCE_DIR}/src/20-bam/availability_builder.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/availability_thread.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/ba.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/ba_status.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/ba_svc_mapping.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/ba_event.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/ba_duration_event.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_aggregate.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_and.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_binary_operator.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_call.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_constant.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_equal.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_expression.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_less_than.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_metric.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_more_than.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_not.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_not_equal.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_operation.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_or.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_service.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_value.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/bool_xor.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/computable.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/applier/ba.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/applier/bool_expression.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/applier/kpi.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/applier/meta_service.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/applier/state.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/ba.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/bool_expression.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/kpi.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/meta_service.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/reader_v2.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/configuration/state.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/connector.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_ba_event.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_kpi_event.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_ba_bv_relation_event.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_bv_event.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_timeperiod.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_timeperiod_exception.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_timeperiod_exclusion.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_ba_timeperiod_relation.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/dimension_truncate_table_signal.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/exp_builder.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/exp_parser.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/exp_tokenizer.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/factory.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/event_cache_visitor.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/hst_svc_mapping.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/impact_values.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/inherited_downtime.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/kpi.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/kpi_ba.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/kpi_boolexp.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/kpi_event.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/kpi_meta.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/kpi_service.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/kpi_status.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/main.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/meta_service.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/meta_service_status.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/metric_book.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/metric_listener.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/monitoring_stream.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/rebuild.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/reporting_stream.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/service_book.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/service_listener.cc
  ${CMAKE_SOURCE_DIR}/src/20-bam/timeperiod_map.cc
)

target_link_libraries(20-bam CONAN_PKG::spdlog)
set_target_properties(20-bam PROPERTIES COMPILE_FLAGS "${MYSQL_CFLAGS} -fPIC")
set_target_properties(20-bam PROPERTIES PREFIX "")
install(TARGETS 20-bam COMPONENT core DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/centreon/lib/centreon-broker)
