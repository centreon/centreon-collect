/*
** Copyright 2011-2013 Centreon
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

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/unified_sql/factory.hh"
#include "com/centreon/broker/unified_sql/index_mapping.hh"
#include "com/centreon/broker/unified_sql/internal.hh"
#include "com/centreon/broker/unified_sql/metric.hh"
#include "com/centreon/broker/unified_sql/metric_mapping.hh"
#include "com/centreon/broker/unified_sql/rebuild.hh"
#include "com/centreon/broker/unified_sql/remove_graph.hh"
#include "com/centreon/broker/unified_sql/status.hh"
#include "com/centreon/broker/unified_sql/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "protobuf/events.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

// Load count.
static uint32_t instances(0);

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
 *  Module deinitialization routine.
 */
void broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    // Deregister unified_sql layer.
    // Remove events.
    io::events::instance().unregister_category(io::storage);
    io::protocols::instance().unreg("storage");
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
    // Storage module.
    log_v2::sql()->info("unified_sql: module for Centreon Broker {}",
                        CENTREON_BROKER_VERSION);

    io::events& e(io::events::instance());

    // Register events.
    {
      e.register_event(make_type(io::storage, storage::de_metric), "metric",
                       &unified_sql::metric::operations,
                       unified_sql::metric::entries, "rt_metrics");
      e.register_event(make_type(io::storage, storage::de_rebuild), "rebuild",
                       &unified_sql::rebuild::operations,
                       unified_sql::rebuild::entries);
      e.register_event(make_type(io::storage, storage::de_remove_graph),
                       "remove_graph", &unified_sql::remove_graph::operations,
                       unified_sql::remove_graph::entries);
      e.register_event(make_type(io::storage, storage::de_status), "status",
                       &unified_sql::status::operations,
                       unified_sql::status::entries);
      e.register_event(make_type(io::storage, storage::de_index_mapping),
                       "index_mapping", &unified_sql::index_mapping::operations,
                       unified_sql::index_mapping::entries);
      e.register_event(make_type(io::storage, storage::de_metric_mapping),
                       "metric_mapping",
                       &unified_sql::metric_mapping::operations,
                       unified_sql::metric_mapping::entries);
      log_v2::bbdo()->info("registering protobuf pb_rebuild as {:x}:{:x}",
                           io::storage, storage::de_pb_rebuild);
      e.register_event(storage_pb_rebuild, "pb_rebuild",
                       &unified_sql::pb_rebuild::operations);
    }

    // Register unified_sql layer.
    io::protocols::instance().reg(
        "unified_sql", std::make_shared<unified_sql::factory>(), 1, 7);
  }
}
}
