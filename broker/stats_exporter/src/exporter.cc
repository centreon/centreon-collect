/*
** Copyright 2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/stats/center.hh"
#include "com/centreon/broker/stats_exporter/exporter.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::stats_exporter;
namespace metric_sdk = opentelemetry::sdk::metrics;

static void update_threadpool_size(metrics_api::ObserverResult observer, void * /* state */) {
  std::lock_guard<stats::center> lck(stats::center::instance());
  const auto& tp = stats::center::instance().threadpool();

  auto observer_long =
    opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(observer);
  observer_long->Observe(static_cast<int64_t>(tp.size()));
}

static void update_threadpool_latency(metrics_api::ObserverResult observer, void * /* state */) {
  std::lock_guard<stats::center> lck(stats::center::instance());
  const auto& tp = stats::center::instance().threadpool();

  auto observer_long =
    opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer);
  observer_long->Observe(static_cast<double>(tp.latency()));
}

exporter::exporter(const std::string& url) : _url(url) {
  opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions otlpOptions;
  /* url should be of the form http://XX.XX.XX.XX:4318/v1/metrics */
  otlpOptions.url = url;
  otlpOptions.aggregation_temporality = opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
  auto exporter =
      opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
          otlpOptions);
  // Initialize and set the periodic metrics reader
  metric_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis = std::chrono::milliseconds(1000);
  options.export_timeout_millis = std::chrono::milliseconds(500);
  std::unique_ptr<metric_sdk::MetricReader> reader{
        new metric_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                      options)};

  // Initialize meter provider
  auto provider = std::shared_ptr<metrics_api::MeterProvider>(
      new metric_sdk::MeterProvider());
  auto p = std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(provider);

  auto meter = provider->GetMeter("broker_stats_threadpool");
  _thread_pool_size = meter->CreateInt64ObservableGauge("size", "Number of threads in the thread pool");
  _thread_pool_size->AddCallback(update_threadpool_size, nullptr);

  _thread_pool_latency = meter->CreateDoubleObservableGauge("latency", "Latency of the thread pool in seconds, the duration to wait for before a thread to be available.");
  _thread_pool_latency->AddCallback(update_threadpool_latency, nullptr);
}
