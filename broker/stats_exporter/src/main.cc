/*
 * Copyright 2023 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>

#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/stats_exporter/exporter_grpc.hh"
#include "com/centreon/broker/stats_exporter/exporter_http.hh"

namespace metric_sdk = opentelemetry::sdk::metrics;
namespace metrics_api = opentelemetry::metrics;

using namespace com::centreon::broker;

// Load count.
static uint32_t instances = 0;

std::unique_ptr<stats_exporter::exporter> expt;

// static void export_to_http(const std::string& url) {
//  opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions otlpOptions;
//  /* url should be of the form http://XX.XX.XX.XX:4318/v1/metrics */
//  otlpOptions.url = url;
//  otlpOptions.aggregation_temporality =
//  opentelemetry::sdk::metrics::AggregationTemporality::kCumulative; auto
//  exporter =
//      opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
//          otlpOptions);
//  // Initialize and set the periodic metrics reader
//  metric_sdk::PeriodicExportingMetricReaderOptions options;
//  options.export_interval_millis = std::chrono::milliseconds(1000);
//  options.export_timeout_millis = std::chrono::milliseconds(500);
//  std::unique_ptr<metric_sdk::MetricReader> reader{
//        new metric_sdk::PeriodicExportingMetricReader(std::move(exporter),
//                                                      options)};
//
//  // Initialize meter provider
//  auto provider = std::shared_ptr<metrics_api::MeterProvider>(
//      new metric_sdk::MeterProvider());
//  auto p = std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
//  p->AddMetricReader(std::move(reader));
//  metrics_api::Provider::SetMeterProvider(provider);
//
//  auto meter = provider->GetMeter("broker_stats_threadpool");
//  s = meter->CreateInt64ObservableGauge("size", "Number of threads in the
//  thread pool"); s->AddCallback(update_threadpool_size, nullptr);

// auto s = meter->CreateUInt64Counter("size", "Number of threads in the thread
// pool"); s->Add(tp.size());

//    auto updown_counter = meter->CreateInt64UpDownCounter("toto_le_hero", "a
//    well known hero"); updown_counter->Add(1); updown_counter->Add(-5);
//      auto test_gauge = meter->CreateDoubleObservableGauge(
//          "test_gauge", "a gauge to test", "p");
//      test_gauge->AddCallback(my_cb, nullptr);
//      // Create Gauge
//      for (int i = 0; i < 100; i++) {
//        log_v2::core()->error("1 second");
//        sleep(1);
//      }
//}

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
const char* broker_module_version = CENTREON_BROKER_VERSION;

/**
 * @brief Return an array with modules needed for this one to work.
 *
 * @return An array of const char*
 */
const char* const* broker_module_parents() {
  constexpr static const char* retval[]{"10-neb.so", nullptr};
  return retval;
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration argument.
 */
void broker_module_init(const void* arg) {
  // Increment instance number.
  if (!instances++) {
    const config::state* s = static_cast<const config::state*>(arg);
    const auto& conf = s->get_stats_exporter();
    const auto& exporters = conf.exporters;
    // Stats module.
    log_v2::config()->info("stats_exporter: module for Centreon Broker {}",
                           CENTREON_BROKER_VERSION);

    for (const auto& e : exporters) {
      log_v2::config()->info("stats_exporter: with exporter '{}'", e.protocol);
      if (e.protocol == "http") {
        expt = std::make_unique<stats_exporter::exporter_http>(e.url, *s);
      } else if (e.protocol == "grpc") {
        expt = std::make_unique<stats_exporter::exporter_grpc>(e.url, *s);
      }
    }
  }
}

/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    expt.reset();
  }
  return true;  // ok to be unloaded
}
}
