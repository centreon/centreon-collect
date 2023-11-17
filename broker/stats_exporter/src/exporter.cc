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
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/sql/mysql_manager.hh"
#include "com/centreon/broker/stats/center.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::stats_exporter;
namespace metric_sdk = opentelemetry::sdk::metrics;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 * @brief Default constructor.
 */
exporter::exporter() : _connections_watcher{pool::io_context()} {}

/**
 * @brief Initialize the metrics to export.
 *
 * @param exporter The exporter coming from exporter_http or exporter_grpc.
 * @param s The exporter configuration.
 */
void exporter::init_metrics(
    std::unique_ptr<metric_sdk::PushMetricExporter>& exporter,
    const config::state& s) {
  double interval = s.get_stats_exporter().export_interval;
  double timeout = s.get_stats_exporter().export_timeout;

  // Initialize and set the periodic metrics reader
  log_v2::instance()
      .get(log_v2::CONFIG)
      ->info(
          "stats_exporter: export configured with an interval of {}s and a "
          "timeout "
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

  /* each instrument is defined that way:
   * provider is given by the opentelemetry library (see above)
   * the name of the instrument
   * a description of the instrument
   * a lambda that just gets the interesting value.
   */
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

  _connections_watcher.expires_after(std::chrono::seconds(10));
  _connections_watcher.async_wait(
      [this, provider](const boost::system::error_code& err) {
        _check_connections(provider, err);
      });
}

/**
 * @brief Destructor.
 */
exporter::~exporter() noexcept {
  boost::system::error_code ec;
  _connections_watcher.cancel(ec);
}

void exporter::_check_connections(
    std::shared_ptr<metrics_api::MeterProvider> provider,
    const boost::system::error_code& ec) {
  if (ec) {
    auto logger = log_v2::instance().get(log_v2::SQL);
    logger->error(
        "stats_exporter: Sql connections checker has been interrupted: {}",
        ec.message());
  } else {
    size_t count = mysql_manager::instance().connections_count();
    while (_conn.size() < count) {
      _conn.push_back({});
      sql_connection& ci = *_conn.rbegin();
      int32_t id = _conn.size() - 1;
      ci.waiting_tasks = std::make_unique<instrument_i64>(
          provider, fmt::format("sql_connection_{}_waiting_tasks", id),
          fmt::format("Number of waiting tasks on the connection {}", id),
          [id]() -> int64_t {
            const auto& s = stats::center::instance().stats();
            if (id < s.sql_manager().connections().size())
              return s.sql_manager().connections().at(id).waiting_tasks();
            else
              return 0;
          });

      ci.loop_duration = std::make_unique<instrument_f64>(
          provider, fmt::format("sql_connection_{}_loop_duration", id),
          fmt::format(
              "Average duration in seconds of one loop on the connection {}",
              id),
          [id]() -> double {
            const auto& s = stats::center::instance().stats();
            if (id < s.sql_manager().connections().size())
              return s.sql_manager()
                  .connections()
                  .at(id)
                  .average_loop_duration();
            else
              return 0;
          });

      ci.average_tasks_count = std::make_unique<instrument_f64>(
          provider, fmt::format("sql_connection_{}_average_tasks_count", id),
          fmt::format("Average number of waiting tasks on the connection {}",
                      id),
          [id]() -> double {
            const auto& s = stats::center::instance().stats();
            if (id < s.sql_manager().connections().size())
              return s.sql_manager().connections().at(id).average_tasks_count();
            else
              return 0;
          });

      ci.activity_percent = std::make_unique<instrument_f64>(
          provider, fmt::format("sql_connection_{}_activity_percent", id),
          fmt::format("Average activity in percent on the connection {} (work "
                      "duration / total duration)",
                      id),
          [id]() -> double {
            const auto& s = stats::center::instance().stats();
            if (id < s.sql_manager().connections().size())
              return s.sql_manager().connections().at(id).activity_percent();
            else
              return 0;
          });

      ci.average_query_duration = std::make_unique<instrument_f64>(
          provider, fmt::format("sql_connection_{}_average_query_duration", id),
          fmt::format("Average activity in percent on the connection {} (work "
                      "duration / total duration)",
                      id),
          [id]() -> double {
            const auto& s = stats::center::instance().stats();
            if (id < s.sql_manager().connections().size())
              return s.sql_manager()
                  .connections()
                  .at(id)
                  .average_query_duration();
            else
              return 0;
          });

      ci.average_statement_duration = std::make_unique<instrument_f64>(
          provider,
          fmt::format("sql_connection_{}_average_statement_duration", id),
          fmt::format("Average activity in percent on the connection {} (work "
                      "duration / total duration)",
                      id),
          [id]() -> double {
            const auto& s = stats::center::instance().stats();
            if (id < s.sql_manager().connections().size())
              return s.sql_manager()
                  .connections()
                  .at(id)
                  .average_statement_duration();
            else
              return 0;
          });
    }
    if (_conn.size() > count)
      _conn.resize(count);

    _connections_watcher.expires_after(std::chrono::seconds(10));
    _connections_watcher.async_wait(
        [this, provider](const boost::system::error_code& err) {
          _check_connections(provider, err);
        });
  }
}
