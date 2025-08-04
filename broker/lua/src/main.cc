/**
 * Copyright 2017-2024 Centreon
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

#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/lua/factory.hh"
#include "com/centreon/broker/lua/stream.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using log_v2 = com::centreon::common::log_v2::log_v2;

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
  constexpr static const char* retval[]{"10-neb.so", "20-bam.so", nullptr};
  return retval;
}

/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    // Unregister generic lua module.
    io::protocols::instance().unreg("lua");
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

  auto logger = log_v2::instance().get(log_v2::LUA);
  // Increment instance number.
  if (!instances++) {
    // generic lua module.
    logger->info("lua: module for Centreon Broker {}", CENTREON_BROKER_VERSION);

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

      /* Let's register the pb_index_mapping event. */
      e.register_event(make_type(io::storage, storage::de_pb_index_mapping),
                       "pb_index_mapping",
                       &storage::pb_index_mapping::operations);
      /* Let's register the pb_metric_mapping event. */
      e.register_event(make_type(io::storage, storage::de_pb_metric_mapping),
                       "pb_metric_mapping",
                       &storage::pb_metric_mapping::operations);
      e.register_event(bam::pb_dimension_bv_event::static_type(),
                       "DimensionBvEvent",
                       &bam::pb_dimension_bv_event::operations);
      e.register_event(bam::pb_dimension_ba_bv_relation_event::static_type(),
                       "DimensionBaBvRelationEvent",
                       &bam::pb_dimension_ba_bv_relation_event::operations);
      e.register_event(bam::pb_dimension_timeperiod::static_type(),
                       "DimensionTimePeriod",
                       &bam::pb_dimension_timeperiod::operations);
      e.register_event(bam::pb_dimension_ba_event::static_type(),
                       "DimensionBaEvent",
                       &bam::pb_dimension_ba_event::operations);
      e.register_event(bam::pb_dimension_truncate_table_signal::static_type(),
                       "DimensionTruncateTableSignal",
                       &bam::pb_dimension_truncate_table_signal::operations);
    }

    // Register lua layer.
    io::protocols::instance().reg("lua", std::make_shared<lua::factory>(), 1,
                                  7);
  }
}
}
