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

#ifndef CCB_STATS_EXPORTER_EXPORTER_HH
#define CCB_STATS_EXPORTER_EXPORTER_HH

#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>

#include "broker.pb.h"
#include "com/centreon/broker/config/state.hh"

namespace metric_sdk = opentelemetry::sdk::metrics;
namespace metrics_api = opentelemetry::metrics;

namespace com::centreon::broker {

namespace stats_exporter {
/**
 * @brief Export stats to an opentelemetry collector. This class is not
 * instanciated directly but through exporter_grpc or exporter_http.
 *
 * The opentelemetry library makes things almost alone, we don't need to add
 * many thing to make it to work. So what does this exporter do?
 *
 * In fact, to export data, we use ObservableGauges, these objects live
 * their own life and we don't need to interract with them except they need
 * a callback to be updated. And then, we need somewhere to store the callback.
 * That's almost this class role. We have two kinds of Gauges, those that
 * store integers and those that store doubles. For each one, we created a
 * little class to store the object, its properties and its callback. And all
 * the class is just a collection of unique_ptr to instrument_f64s and
 * instrument_i64s.
 */
class exporter {
  /**
   * @brief Class to work on ObservableGauge with double values.
   */
  class instrument_f64 {
    /**
     * @brief The gauge to feed with double values. It lives its own life, and
     * calls the callback when needed.
     */
    opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument> _inst;
    /**
     * @brief The function we write to get the value for the gauge. This
     * function takes no parameter and returns a double.
     */
    std::function<double()> _query;

    /**
     * @brief The callback of the Gauge. The global code is always the same,
     * but the specificity to get the interesting value is done by the
     * _query() function.
     *
     * @param observer The result to set from the update_double() function.
     * @param state Some user data. We use it to get the _query function.
     */
    static void update_double(metrics_api::ObserverResult observer,
                              void* state) {
      std::function<double()> query =
          *reinterpret_cast<std::function<double()>*>(state);
      std::lock_guard<stats::center> lck(stats::center::instance());

      auto observer_long =
          opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
              opentelemetry::metrics::ObserverResultT<double>>>(observer);
      observer_long->Observe(static_cast<double>(query()));
    }

   public:
    instrument_f64(std::shared_ptr<metrics_api::MeterProvider>& provider,
                   const std::string& name,
                   const std::string& description,
                   std::function<double()>&& query)
        : _query(query) {
      auto meter = provider->GetMeter("broker_stats_threadpool");
      _inst = meter->CreateDoubleObservableGauge(name, description);
      _inst->AddCallback(update_double, &_query);
    }
  };

  /**
   * @brief This class is the same as instrument_f64 but for integer values.
   */
  class instrument_i64 {
    opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument> _inst;
    std::function<int64_t()> _query;

    static void update_int64(metrics_api::ObserverResult observer,
                             void* state) {
      std::function<int64_t()> query =
          *reinterpret_cast<std::function<int64_t()>*>(state);
      std::lock_guard<stats::center> lck(stats::center::instance());

      auto observer_long =
          opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
              opentelemetry::metrics::ObserverResultT<int64_t>>>(observer);
      observer_long->Observe(static_cast<int64_t>(query()));
    }

   public:
    instrument_i64(std::shared_ptr<metrics_api::MeterProvider>& provider,
                   const std::string& name,
                   const std::string& description,
                   std::function<int64_t()>&& query)
        : _query(query) {
      auto meter = provider->GetMeter("broker_stats_threadpool");
      _inst = meter->CreateInt64ObservableGauge(name, description);
      _inst->AddCallback(update_int64, &_query);
    }
  };

  /* Thread pool */
  std::unique_ptr<instrument_i64> _thread_pool_size;
  std::unique_ptr<instrument_f64> _thread_pool_latency;

  /* Muxers */
  struct queue_file_instrument {
    std::unique_ptr<instrument_i64> file_write_path;
    std::unique_ptr<instrument_i64> file_read_path;
    std::unique_ptr<instrument_f64> file_percent_processed;
  };

  struct muxer_instrument {
    std::unique_ptr<instrument_i64> total_events;
    std::unique_ptr<instrument_i64> unacknowledged_events;
    queue_file_instrument queue_file;
  };
  std::vector<muxer_instrument> _muxer;

  /* Mysql */
  struct sql_connection {
    std::unique_ptr<instrument_i64> waiting_tasks;
    std::unique_ptr<instrument_f64> loop_duration;
    std::unique_ptr<instrument_f64> average_tasks_count;
    std::unique_ptr<instrument_f64> activity_percent;
    std::unique_ptr<instrument_f64> average_query_duration;
    std::unique_ptr<instrument_f64> average_statement_duration;
  };
  std::vector<sql_connection> _conn;

  /**
   * @brief This is useful when mysql connections change, we can update
   * observers.
   */
  asio::steady_timer _connections_watcher;

  void _check_connections(std::shared_ptr<metrics_api::MeterProvider> provider,
                          const boost::system::error_code& ec);

 public:
  exporter();
  virtual ~exporter() noexcept;

  void init_metrics(std::unique_ptr<metric_sdk::PushMetricExporter>& exporter,
                    const config::state& s);
};

}  // namespace stats_exporter

}

#endif /* !CCB_STATS_EXPORTER_EXPORTER_HH */
