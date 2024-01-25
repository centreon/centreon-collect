/**
* Copyright 2011-2013, 2020-2022 Centreon
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

#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "com/centreon/broker/influxdb/factory.hh"
#include "com/centreon/broker/influxdb/internal.hh"
#include "com/centreon/broker/influxdb/stream.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker;

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
  if (!--instances) {
    // Deregister storage layer.
    io::protocols::instance().unreg("influxdb");
  }
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
    // Storage module.
    log_v2::influxdb()->info("influxdb: module for Centreon Broker {}",
                             CENTREON_BROKER_VERSION);

    io::events& e(io::events::instance());

    // Register events.
    {
      e.register_event(make_type(io::storage, storage::de_metric), "metric",
                       &storage::metric::operations, storage::metric::entries,
                       "rt_metrics");
      e.register_event(make_type(io::storage, storage::de_status), "status",
                       &storage::status::operations, storage::status::entries);
      e.register_event(make_type(io::storage, storage::de_index_mapping),
                       "index_mapping", &storage::index_mapping::operations,
                       storage::index_mapping::entries);
      e.register_event(make_type(io::storage, storage::de_metric_mapping),
                       "metric_mapping", &storage::metric_mapping::operations,
                       storage::metric_mapping::entries);

      /* Let's register the pb_index_mapping event. */
      e.register_event(make_type(io::storage, storage::de_pb_index_mapping),
                       "pb_index_mapping",
                       &storage::pb_index_mapping::operations);

      /* Let's register the pb_metric_mapping event. */
      e.register_event(make_type(io::storage, storage::de_pb_metric_mapping),
                       "pb_metric_mapping",
                       &storage::pb_metric_mapping::operations);
      /* Let's register the pb_metric event. */
      e.register_event(make_type(io::storage, storage::de_pb_metric),
                       "pb_metric", &storage::pb_metric::operations);
      /* Let's register the pb_status event. */
      e.register_event(make_type(io::storage, storage::de_pb_status),
                       "pb_status", &storage::pb_status::operations);
    }

    // Register storage layer.
    io::protocols::instance().reg("influxdb",
                                  std::make_shared<influxdb::factory>(), 1, 7);
  }
}
}
