/*
** Copyright 2017-2021 Centreon
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

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/lua/factory.hh"
#include "com/centreon/broker/lua/stream.hh"

using namespace com::centreon::broker;

// Load count.
static uint32_t instances{0u};

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
void broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    // Unregister generic lua module.
    io::protocols::instance().unreg("lua");
  }
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
    // generic lua module.
    log_v2::lua()->info("lua: module for Centreon Broker {}",
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
      e.register_event(
          make_type(io::bam, bam::de_dimension_ba_bv_relation_event),
          "dimension_ba_bv_relation_event",
          &bam::dimension_ba_bv_relation_event::operations,
          bam::dimension_ba_bv_relation_event::entries);
      e.register_event(make_type(io::bam, bam::de_dimension_ba_event),
                       "dimension_ba_event",
                       &bam::dimension_ba_event::operations,
                       bam::dimension_ba_event::entries);
      e.register_event(make_type(io::bam, bam::de_dimension_bv_event),
                       "dimension_bv_event",
                       &bam::dimension_bv_event::operations,
                       bam::dimension_bv_event::entries);
      e.register_event(
          make_type(io::bam, bam::de_dimension_truncate_table_signal),
          "dimension_truncate_table_signal",
          &bam::dimension_truncate_table_signal::operations,
          bam::dimension_truncate_table_signal::entries);
    }

    // Register lua layer.
    io::protocols::instance().reg("lua", std::make_shared<lua::factory>(), 1,
                                  7);
  }
}
}
