/**
 * Copyright 2023 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/broker/stats_exporter/exporter_grpc.hh"
#include "com/centreon/broker/stats_exporter/exporter.hh"

using namespace com::centreon::broker::stats_exporter;

exporter_grpc::exporter_grpc(const std::string& url, const config::state& s)
    : exporter(), _url{url} {
  opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions otlpOptions;
  /* url should be of the form http://XX.XX.XX.XX:4317 */
  otlpOptions.endpoint = url;
  otlpOptions.aggregation_temporality =
      opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
  auto exporter =
      opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
          otlpOptions);

  init_metrics(exporter, s);
}
