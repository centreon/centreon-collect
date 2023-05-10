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

#include "com/centreon/broker/stats_exporter/exporter.hh"
#include "com/centreon/broker/config/endpoint.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/stats/center.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::stats_exporter;
namespace metric_sdk = opentelemetry::sdk::metrics;

static void update_threadpool_size(metrics_api::ObserverResult observer,
                                   void* /* state */) {
  std::lock_guard<stats::center> lck(stats::center::instance());
  const auto& tp = stats::center::instance().threadpool();

  auto observer_long =
      opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObserverResultT<int64_t>>>(observer);
  observer_long->Observe(static_cast<int64_t>(tp.size()));
}

static void update_threadpool_latency(metrics_api::ObserverResult observer,
                                      void* /* state */) {
  std::lock_guard<stats::center> lck(stats::center::instance());
  const auto& tp = stats::center::instance().threadpool();

  auto observer_long =
      opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObserverResultT<double>>>(observer);
  observer_long->Observe(static_cast<double>(tp.latency()));
}

static void update_muxer_unacknowledged_events(
    metrics_api::ObserverResult observer,
    void* state) {
  const char* name = reinterpret_cast<const char*>(state);
  std::lock_guard<stats::center> lck(stats::center::instance());
  const auto& s = stats::center::instance().stats();
  const auto& m = s.processing().muxers().at(name);

  auto observer_long =
      opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObserverResultT<int64_t>>>(observer);
  observer_long->Observe(static_cast<int64_t>(m.unacknowledged_events()));
}

exporter::exporter(const std::string& url, const config::state& s) : _url(url) {
  double interval = s.get_stats_exporter().export_interval;
  double timeout = s.get_stats_exporter().export_timeout;

  opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions otlpOptions;
  /* url should be of the form http://XX.XX.XX.XX:4318/v1/metrics */
  otlpOptions.url = url;
  otlpOptions.aggregation_temporality =
      opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
  auto exporter =
      opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
          otlpOptions);
  // Initialize and set the periodic metrics reader
  log_v2::config()->info(
      "stats_exporter: export configured with an interval of {}s and a timeout "
      "of {}s",
      interval, timeout);
  metric_sdk::PeriodicExportingMetricReaderOptions options;
  options.export_interval_millis =
      std::chrono::milliseconds(static_cast<int32_t>(1000. * interval));
  options.export_timeout_millis =
      std::chrono::milliseconds(static_cast<int32_t>(1000. * timeout));
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
  _thread_pool_size = std::make_unique<instrument_i64>(
      provider, "thread_pool_size", "Number of threads in the thread pool",
      []() -> int64_t {
        const auto& s = stats::center::instance().stats().pool_stats();
        return s.size();
      });

  _thread_pool_latency = std::make_unique<instrument_f64>(
      provider, "thread_pool_latency",
      "Latency of the thread pool in seconds, the time to wait before a thread "
      "is available",
      []() -> double {
        const auto& s = stats::center::instance().stats().pool_stats();
        return s.latency();
      });

  for (auto& m : s.endpoints()) {
    if (m.get_io_type() == config::endpoint::output) {
      _muxer.push_back({});
      muxer_instrument& mi = *_muxer.rbegin();
      mi.total_events = std::make_unique<instrument_i64>(
          provider, fmt::format("{}_muxer_total_events", m.name),
          fmt::format("Total number of events stacked in the muxer '{}'",
                      m.name),
          [name = m.name]() -> int64_t {
            const auto& s = stats::center::instance().stats();
            return s.processing().muxers().at(name).total_events();
          });

      mi.unacknowledged_events = std::make_unique<instrument_i64>(
          provider, fmt::format("{}_muxer_unacknowledged_events", m.name),
          fmt::format(
              "Number of unacknowlkedged events stacked in the muxer '{}'",
              m.name),
          [name = m.name]() -> int64_t {
            const auto& s = stats::center::instance().stats();
            return s.processing().muxers().at(name).unacknowledged_events();
          });

      mi.queue_file.file_write_path = std::make_unique<instrument_i64>(
          provider, fmt::format("{}_muxer_queue_file_write_path", m.name),
          fmt::format("Index of the current written queue file for muxer '{}'",
                      m.name),
          [name = m.name]() -> int64_t {
            const auto& s = stats::center::instance().stats();
            const auto& q = s.processing().muxers().at(name).queue_file();
            return q.file_write_path();
          });

      mi.queue_file.file_read_path = std::make_unique<instrument_i64>(
          provider, fmt::format("{}_muxer_queue_file_read_path", m.name),
          fmt::format("Index of the current written queue file for muxer '{}'",
                      m.name),
          [name = m.name]() -> int64_t {
            const auto& s = stats::center::instance().stats();
            const auto& q = s.processing().muxers().at(name).queue_file();
            return q.file_read_path();
          });

      mi.queue_file.file_percent_processed = std::make_unique<instrument_f64>(
          provider,
          fmt::format("{}_muxer_queue_file_percent_processed", m.name),
          fmt::format("percentage progression of the retention reading of the "
                      "muxer '{}'",
                      m.name),
          [name = m.name]() -> double {
            const auto& s = stats::center::instance().stats();
            const auto& q = s.processing().muxers().at(name).queue_file();
            return q.file_percent_processed();
          });
    }
  }
}
