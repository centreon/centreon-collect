/**
 * Copyright 2011-2013 Centreon
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

#include "bbdo/events.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/unified_sql/factory.hh"
#include "com/centreon/broker/unified_sql/internal.hh"
#include "com/centreon/broker/unified_sql/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

using log_v2 = com::centreon::common::log_v2::log_v2;

// Load count.
static uint32_t instances{0u};

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
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    // Deregister unified_sql layer.
    // Remove events.
    io::events::instance().unregister_category(io::storage);
    io::protocols::instance().unreg("storage");
  }
  return true;  // ok to be unloaded
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(void const* arg [[maybe_unused]]) {
  auto logger = log_v2::instance().get(log_v2::CONFIG);

  // Increment instance number.
  if (!instances++) {
    // Storage module.
    logger->info("unified_sql: module for Centreon Broker {}",
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

      /* Let's register the rebuild_metrics bbdo event. This is needed to send
       * the rebuild message from the gRPC interface. */
      e.register_event(make_type(io::bbdo, bbdo::de_rebuild_graphs),
                       "rebuild_metrics", &bbdo::pb_rebuild_graphs::operations);

      /* Let's register the message to start rebuilds, send rebuilds and
       * terminate rebuilds. This is pb_rebuild_message. */
      e.register_event(make_type(io::storage, storage::de_rebuild_message),
                       "RebuildMessage",
                       &storage::pb_rebuild_message::operations);

      /* Let's register the pb_remove_graphs bbdo event. This is needed to send
       * the remove graphs message from the gRPC interface. */
      e.register_event(make_type(io::bbdo, bbdo::de_remove_graphs),
                       "remove_graphs", &bbdo::pb_remove_graphs::operations);

      /* Let's register the message to ask rrd for remove metrics. This is
       * pb_remove_graph_message. */
      e.register_event(make_type(io::storage, storage::de_remove_graph_message),
                       "RemoveGraphMessage",
                       &storage::pb_remove_graph_message::operations);

      /* Let's register the remove_poller event. */
      e.register_event(make_type(io::bbdo, bbdo::de_remove_poller),
                       "remove_poller", &bbdo::pb_remove_poller::operations);

      /* Let's register the pb_metric event. */
      e.register_event(make_type(io::storage, storage::de_pb_metric),
                       "pb_metric", &storage::pb_metric::operations);

      /* Let's register the pb_status event. */
      e.register_event(make_type(io::storage, storage::de_pb_status),
                       "pb_status", &storage::pb_status::operations);

      /* Let's register the pb_index_mapping event. */
      e.register_event(make_type(io::storage, storage::de_pb_index_mapping),
                       "pb_index_mapping",
                       &storage::pb_index_mapping::operations);

      /* Let's register the pb_metric_mapping event. */
      e.register_event(make_type(io::storage, storage::de_pb_metric_mapping),
                       "pb_metric_mapping",
                       &storage::pb_metric_mapping::operations);

      /* Let's register the message received when a poller is stopped. This is
       * pb_stop. */
      e.register_event(make_type(io::local, local::de_pb_stop), "LocStop",
                       &local::pb_stop::operations);
    }

    // Register unified_sql layer.
    io::protocols::instance().reg(
        "unified_sql", std::make_shared<unified_sql::factory>(), 1, 7);
  }
}
}
