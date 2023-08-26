/**
* Copyright 2022 Centreon
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

#include <rrd.h>

#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/http_tsdb/internal.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/victoria_metrics/factory.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v3 = com::centreon::common::log_v3::log_v3;

// Load count.
static uint32_t instances(0);

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
char const* broker_module_version = CENTREON_BROKER_VERSION;

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
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances)
    // Deregister RRD layer.
    io::protocols::instance().unreg("VICTORIA_METRICS");
  return true;  // ok to be unloaded
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(void const* arg) {
  (void)arg;

  // Increment instance number.
  if (!instances++) {
    // RRD module.
    SPDLOG_LOGGER_INFO(log_v3::instance().get(0),
                       "victoria_metrics: module for Centreon Broker {}",
                       CENTREON_BROKER_VERSION);

    io::events& e(io::events::instance());

    // Register events.
    {
      e.register_event(make_type(io::storage, storage::de_metric), "metric",
                       &storage::metric::operations, storage::metric::entries,
                       "rt_metrics");
      e.register_event(make_type(io::storage, storage::de_status), "status",
                       &storage::status::operations, storage::status::entries);

      e.register_event(make_type(io::storage, storage::de_pb_metric),
                       "pb_metric", &storage::pb_metric::operations);
      e.register_event(make_type(io::storage, storage::de_pb_status),
                       "pb_status", &storage::pb_status::operations);
    }

    // Register  layer.
    io::protocols::instance().reg("VICTORIA_METRICS",
                                  std::make_shared<victoria_metrics::factory>(),
                                  1, 7);
    cache::global_cache::load(fmt::format(
        "{}.cache.global", config::applier::state::instance().cache_dir()));
  }
}
}
