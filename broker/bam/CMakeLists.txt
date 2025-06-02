#
# Copyright 2011-2023 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#

# Global options.
set(INC_DIR "${PROJECT_SOURCE_DIR}/bam/inc")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/bam/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/bam/test")
include_directories(
  ${INC_DIR} ${PROJECT_SOURCE_DIR}/core/inc ${PROJECT_SOURCE_DIR}/core/sql/inc
  ${PROJECT_SOURCE_DIR}/neb/inc
  ${PROJECT_SOURCE_DIR}/core/src
)

set(INC_DIR "${INC_DIR}/com/centreon/broker/bam")

# BAM module.
set(BAM "20-bam")
set(BAM
    "${BAM}"
    PARENT_SCOPE)
add_library(
  "${BAM}" SHARED
  ${SRC_DIR}/availability_builder.cc
  ${SRC_DIR}/availability_thread.cc
  ${SRC_DIR}/ba.cc
  ${SRC_DIR}/ba_best.cc
  ${SRC_DIR}/ba_impact.cc
  ${SRC_DIR}/ba_ratio_number.cc
  ${SRC_DIR}/ba_ratio_percent.cc
  ${SRC_DIR}/ba_worst.cc
  ${SRC_DIR}/ba_svc_mapping.cc
  ${SRC_DIR}/bool_and.cc
  ${SRC_DIR}/bool_binary_operator.cc
  ${SRC_DIR}/bool_call.cc
  ${SRC_DIR}/bool_constant.cc
  ${SRC_DIR}/bool_equal.cc
  ${SRC_DIR}/bool_expression.cc
  ${SRC_DIR}/bool_less_than.cc
  ${SRC_DIR}/bool_more_than.cc
  ${SRC_DIR}/bool_not.cc
  ${SRC_DIR}/bool_not_equal.cc
  ${SRC_DIR}/bool_operation.cc
  ${SRC_DIR}/bool_or.cc
  ${SRC_DIR}/bool_service.cc
  ${SRC_DIR}/bool_value.cc
  ${SRC_DIR}/bool_xor.cc
  ${SRC_DIR}/computable.cc
  ${SRC_DIR}/configuration/applier/ba.cc
  ${SRC_DIR}/configuration/applier/bool_expression.cc
  ${SRC_DIR}/configuration/applier/kpi.cc
  ${SRC_DIR}/configuration/applier/state.cc
  ${SRC_DIR}/configuration/ba.cc
  ${SRC_DIR}/configuration/bool_expression.cc
  ${SRC_DIR}/configuration/kpi.cc
  ${SRC_DIR}/configuration/reader_v2.cc
  ${SRC_DIR}/configuration/state.cc
  ${SRC_DIR}/connector.cc
  ${SRC_DIR}/exp_builder.cc
  ${SRC_DIR}/exp_parser.cc
  ${SRC_DIR}/exp_tokenizer.cc
  ${SRC_DIR}/factory.cc
  ${SRC_DIR}/event_cache_visitor.cc
  ${SRC_DIR}/hst_svc_mapping.cc
  ${SRC_DIR}/impact_values.cc
  ${SRC_DIR}/kpi.cc
  ${SRC_DIR}/kpi_ba.cc
  ${SRC_DIR}/kpi_boolexp.cc
  ${SRC_DIR}/kpi_service.cc
  ${SRC_DIR}/main.cc
  ${SRC_DIR}/monitoring_stream.cc
  ${SRC_DIR}/reporting_stream.cc
  ${SRC_DIR}/service_book.cc
  ${SRC_DIR}/service_listener.cc
  ${SRC_DIR}/timeperiod_map.cc)
target_link_libraries("${BAM}" PRIVATE bbdo_storage bbdo_bam pb_bam_lib
                      spdlog::spdlog)
target_precompile_headers("${BAM}" REUSE_FROM multiplexing)
set_target_properties("${BAM}" PROPERTIES PREFIX ""
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")
add_dependencies("${BAM}" pb_open_telemetry_lib pb_bam_lib)

# Testing.
if(WITH_TESTING)
  if(WITH_SQL_TESTS)
    set(TESTS_SOURCES ${TESTS_SOURCES} ${TEST_DIR}/monitoring_stream.cc)
  endif(WITH_SQL_TESTS)

  # Testing.
  set(TESTS_SOURCES
      ${TESTS_SOURCES}
      ${TEST_DIR}/ba/kpi_service.cc
      ${TEST_DIR}/ba/kpi_ba.cc
      ${TEST_DIR}/configuration/applier-boolexp.cc
      ${TEST_DIR}/exp_builder/exp_builder.cc
      ${TEST_DIR}/exp_builder/availability_builder.cc
      ${TEST_DIR}/exp_parser/get_postfix.cc
      ${TEST_DIR}/exp_tokenizer/next.cc
      ${TEST_DIR}/time/check_timeperiod.cc
      PARENT_SCOPE)
  set(TESTS_LIBRARIES
      ${TESTS_LIBRARIES} ${BAM}
      PARENT_SCOPE)
endif(WITH_TESTING)

# Install rule.
install(TARGETS "${BAM}" LIBRARY DESTINATION "${PREFIX_MODULES}")
