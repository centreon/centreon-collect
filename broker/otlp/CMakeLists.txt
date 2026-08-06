#
# Copyright 2026 Centreon
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
set(INC_DIR "${PROJECT_SOURCE_DIR}/otlp/inc/com/centreon/broker/otlp")
set(SRC_DIR "${PROJECT_SOURCE_DIR}/otlp/src")
set(TEST_DIR "${PROJECT_SOURCE_DIR}/otlp/test")

# common/grpc/inc is not on broker's global include path; each module that
# needs the shared gRPC client base adds it itself.
include_directories(inc
  ${PROJECT_SOURCE_DIR}/neb/inc
  ${PROJECT_SOURCE_DIR}/core/cache/inc
  ${CMAKE_SOURCE_DIR}/common/grpc/inc
)

# Sources.
set(SOURCES
  ${SRC_DIR}/connector.cc
  ${SRC_DIR}/factory.cc
  ${SRC_DIR}/otlp_exporter.cc
  ${SRC_DIR}/request_builder.cc
  ${SRC_DIR}/resource_enricher.cc
  ${SRC_DIR}/semconv_mapping.cc
  ${SRC_DIR}/stream.cc
  ${SRC_DIR}/main.cc
)

# Headers.
set(HEADERS
  ${INC_DIR}/connector.hh
  ${INC_DIR}/factory.hh
  ${INC_DIR}/otlp_config.hh
  ${INC_DIR}/otlp_exporter.hh
  ${INC_DIR}/request_builder.hh
  ${INC_DIR}/resource_enricher.hh
  ${INC_DIR}/semconv_mapping.hh
  ${INC_DIR}/stream.hh
)

# OTLP module.
set(OTLP "70-otlp")
set(OTLP "${OTLP}" PARENT_SCOPE)
add_library("${OTLP}" SHARED
  ${SOURCES}
  ${HEADERS}
)
set_target_properties("${OTLP}" PROPERTIES PREFIX ""
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/broker/lib")

# pb_otel_grpc_lib carries the MetricsService stub; pb_open_telemetry_lib (the
# messages) arrives transitively through centreon_pb_bbdo. Neither tree is
# duplicated into this module: see broker/doc/otlp_output_architecture.md.
add_dependencies("${OTLP}" pb_otel_grpc_lib pb_neb_lib global_cache)

target_link_libraries("${OTLP}"
  pb_otel_grpc_lib
  pb_open_telemetry_lib
  global_cache
  centreon_grpc
  centreon_common
  pb_neb_lib centreon_pb_bbdo fmt::fmt)

target_precompile_headers(${OTLP} PRIVATE precomp_inc/precomp.hh)

# Testing.
if(WITH_TESTING)
  set(
    TESTS_SOURCES
    ${TESTS_SOURCES}
    ${TEST_DIR}/semconv_mapping_test.cc
    ${TEST_DIR}/request_builder_test.cc
    ${TEST_DIR}/stream_test.cc
    ${TEST_DIR}/factory_test.cc
    PARENT_SCOPE
  )
  set(
    TESTS_LIBRARIES
    ${TESTS_LIBRARIES}
    ${OTLP}
    PARENT_SCOPE
  )
endif(WITH_TESTING)

# Install rule.
install(TARGETS "${OTLP}"
  LIBRARY DESTINATION "${PREFIX_MODULES}"
)
