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

set(STATS_EXPORTER "15-stats_exporter")
set(STATS_EXPORTER
    ${STATS_EXPORTER}
    PARENT_SCOPE)
include_directories(inc)

add_library(${STATS_EXPORTER} SHARED src/exporter.cc src/exporter_grpc.cc
                                     src/exporter_http.cc src/main.cc)

set_target_properties(${STATS_EXPORTER} PROPERTIES PREFIX "")

# Be careful: we can't use directly the CONAN_PKG for opentelemetry because of
# the grpc dependency. We know that if we link twice with that library, we get
# deadlocks with it. So here we only get from opentelemetry new stuffs.
target_link_libraries(
  ${STATS_EXPORTER}
  PRIVATE
  ${CURL_LIBRARY_RELEASE}
  spdlog::spdlog
  -L${OTLP_LIB_DIR}
  opentelemetry_version
  opentelemetry_exporter_otlp_grpc
  opentelemetry_exporter_otlp_grpc_metrics
  opentelemetry_exporter_otlp_grpc_client
  opentelemetry_exporter_otlp_http
  opentelemetry_exporter_otlp_http_metric
  opentelemetry_otlp_recordable
  opentelemetry_exporter_otlp_http_client
  opentelemetry_proto
  opentelemetry_http_client_curl
  opentelemetry_resources
  opentelemetry_common
  opentelemetry_metrics)

add_definitions("-DHAVE_ABSEIL")
target_precompile_headers(${STATS_EXPORTER} PRIVATE precomp_inc/precomp.hpp)

# Install rule.
install(TARGETS ${STATS_EXPORTER} LIBRARY DESTINATION "${PREFIX_MODULES}")
