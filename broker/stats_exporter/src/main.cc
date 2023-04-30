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

#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/stats_exporter/parser.hh"
#include "com/centreon/broker/stats_exporter/worker_pool.hh"

using namespace com::centreon::broker;

// Load count.
static uint32_t instances = 0;

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
char const* broker_module_version = CENTREON_BROKER_VERSION;

opentelemetry::exporter::jaeger::JaegerExporterOptions opts;

void init_tracer() {
}

void cleanup_tracer() {
  std::shared_ptr<trace::TracerProvider> none;
  trace::Provider::SetTracerProvider(none);
}

/**
 *  Module deinitialization routine.
 */
void broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    std::shared_ptr<trace::TracerProvider> none;
    trace::Provider::SetTracerProvider(none);
  }
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration argument.
 */
void broker_module_init(void const* arg) {
  // Increment instance number.
  if (!instances++) {
    // Stats module.
    log_v2::sql()->info("stats_export: module for Centreon Broker {}",
                        CENTREON_BROKER_VERSION);

    // Create Jaeger exporter instance
    auto exporter = jaeger::JaegerExporterFactory::Create(opts);
    auto processor =
        trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
    std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
        trace_sdk::TracerProviderFactory::Create(std::move(processor));
    // Set the global trace provider
    trace::Provider::SetTracerProvider(provider);

//    // Check that stats are enabled.
//    const config::state& base_cfg(*static_cast<config::state const*>(arg));
//    bool loaded = false;
//    std::map<std::string, std::string>::const_iterator it =
//        base_cfg.params().find("stats_exporter");
//    if (it != base_cfg.params().end()) {
//      try {
//        // Parse configuration.
//        std::vector<std::string> stats_cfg;
//        {
//          stats_export::parser p;
//          p.parse(stats_cfg, it->second);
//        }
//
//        // File configured, load stats_export engine.
//        for (std::vector<std::string>::const_iterator it = stats_cfg.begin(),
//                                                      end = stats_cfg.end();
//             it != end; ++it) {
//          wpool.add_worker(*it);
//        }
//        loaded = true;
//      } catch (std::exception const& e) {
//        log_v2::stats()->info("stats_export: engine loading failure: {}", e.what());
//      } catch (...) {
//        log_v2::stats()->info("stats_export: engine loading failure");
//      }
//    }
//    if (!loaded) {
//      wpool.cleanup();
//      log_v2::stats()->info(
//          "stats_export: invalid stats "
//          "configuration, stats engine is NOT loaded");
//    }
  }
}
}
