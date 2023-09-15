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
#include "common/log_v2/log_v2.hh"

#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/stats_exporter/exporter_grpc.hh"
#include "com/centreon/broker/stats_exporter/exporter_http.hh"

namespace metric_sdk = opentelemetry::sdk::metrics;
namespace metrics_api = opentelemetry::metrics;

using namespace com::centreon::broker;
using log_v3 = com::centreon::common::log_v3::log_v3;

// Load count.
static uint32_t instances = 0;

std::unique_ptr<stats_exporter::exporter> expt;

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
    log_v3::instance().get(1)->info(
        "stats_exporter: module for Centreon Broker {}",
        CENTREON_BROKER_VERSION);

    for (const auto& e : exporters) {
      log_v3::instance().get(1)->info("stats_exporter: with exporter '{}'",
                                      e.protocol);
      switch (e.protocol) {
        case config::state::stats_exporter::HTTP:
          expt = std::make_unique<stats_exporter::exporter_http>(e.url, *s);
          break;
        case config::state::stats_exporter::GRPC:
          expt = std::make_unique<stats_exporter::exporter_grpc>(e.url, *s);
          break;
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
